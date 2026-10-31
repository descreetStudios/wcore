#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <typeinfo>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <wcore/types.hpp>

namespace wcore
{
	// A type-safe, synchronous event bus.
	class EventBus
	{
	private:
		struct SubscriberBase
		{
			explicit SubscriberBase(u64 id) : id(id) {}
			virtual ~SubscriberBase() = default;

			u64 id;
			bool active{ true };
		};

		template <typename E>
		struct Subscriber final : SubscriberBase
		{
			template <typename Fn>
			Subscriber(u64 id, Fn&& fn)
				: SubscriberBase(id), callback(std::forward<Fn>(fn)) {}

			std::function<void(const E&)> callback;
		};

		struct Channel
		{
			std::vector<std::unique_ptr<SubscriberBase>> subscribers;
			u32 depth{ 0 };

			void cleanup()
			{
				std::erase_if(subscribers,
					[](const auto& subscriber) { return !subscriber->active; });
			}
		};

		struct State
		{
			std::recursive_mutex mutex;
			std::unordered_map<std::type_index, std::shared_ptr<Channel>> channels;
			u64 nextId{ 1 };

			// The caller must hold mutex.
			bool remove(std::type_index type, u64 id)
			{
				auto it = channels.find(type);
				if (it == channels.end()) return false;

				const std::shared_ptr<Channel>& channel{ it->second };
				for (auto subscriber = channel->subscribers.begin();
					subscriber != channel->subscribers.end(); ++subscriber)
				{
					if ((*subscriber)->id != id) continue;

					(*subscriber)->active = false;
					if (channel->depth == 0)
					{
						channel->subscribers.erase(subscriber);
						if (channel->subscribers.empty())
							channels.erase(it);
					}
					return true;
				}
				return false;
			}

			// The caller must hold mutex.
			bool contains(std::type_index type, u64 id) const
			{
				auto it = channels.find(type);
				if (it == channels.end()) return false;

				for (const auto& subscriber : it->second->subscribers)
				{
					if (subscriber->id == id && subscriber->active)
						return true;
				}
				return false;
			}
		};

	public:
		// RAII token for one subscription. Move-only.
		class Subscription
		{
		public:
			Subscription() noexcept = default;
			~Subscription() { unsubscribe(); }

			Subscription(const Subscription&) = delete;
			Subscription& operator=(const Subscription&) = delete;

			Subscription(Subscription&& other) noexcept
				: mState(std::move(other.mState)),
				mType(other.mType),
				mId(other.mId)
			{
				other.mId = 0;
			}

			Subscription& operator=(Subscription&& other) noexcept
			{
				if (this != &other)
				{
					unsubscribe();
					mState = std::move(other.mState);
					mType = other.mType;
					mId = other.mId;
					other.mId = 0;
				}
				return *this;
			}

			// Stops receiving events. Safe to call more than once.
			void unsubscribe()
			{
				if (mId == 0) return;

				if (const std::shared_ptr<State> state{ mState.lock() })
				{
					std::lock_guard<std::recursive_mutex> lock{ state->mutex };
					state->remove(mType, mId);
				}

				mState.reset();
				mId = 0;
			}

			// True while the subscription is registered with a live bus.
			bool active() const
			{
				if (mId == 0) return false;

				const std::shared_ptr<State> state{ mState.lock() };
				if (!state) return false;

				std::lock_guard<std::recursive_mutex> lock{ state->mutex };
				return state->contains(mType, mId);
			}

			explicit operator bool() const { return active(); }

		private:
			friend class EventBus;

			Subscription(std::weak_ptr<State> state,
				std::type_index type, u64 id) noexcept
				: mState(std::move(state)), mType(type), mId(id) {}

			std::weak_ptr<State> mState;
			std::type_index mType{ typeid(void) };
			u64 mId{ 0 };
		};

		EventBus() = default;
		~EventBus() = default;

		EventBus(const EventBus&) = delete;
		EventBus& operator=(const EventBus&) = delete;
		EventBus(EventBus&&) = delete;
		EventBus& operator=(EventBus&&) = delete;

		// Registers a handler for events of type E.
		template <typename E, typename Fn>
		[[nodiscard]] Subscription subscribe(Fn&& fn)
		{
			static_assert(std::is_invocable_v<Fn&, const E&>,
				"Event handler must be callable with const E&");

			std::lock_guard<std::recursive_mutex> lock{ mState->mutex };

			const std::type_index type{ typeid(E) };
			std::shared_ptr<Channel>& channel{ mState->channels[type] };
			if (!channel) channel = std::make_shared<Channel>();

			const u64 id{ mState->nextId++ };
			channel->subscribers.push_back(
				std::make_unique<Subscriber<E>>(id, std::forward<Fn>(fn)));

			return Subscription{ mState, type, id };
		}

		// Invokes every handler registered for E and returns how many ran.
		// Handlers may publish nested events, subscribe and unsubscribe.
		template <typename E>
		std::size_t publish(const E& event)
		{
			std::lock_guard<std::recursive_mutex> lock{ mState->mutex };

			auto it = mState->channels.find(typeid(E));
			if (it == mState->channels.end()) return 0;

			// Keep the channel alive even if a handler calls clear().
			const std::shared_ptr<Channel> channel{ it->second };
			++channel->depth;

			std::size_t called{ 0 };
			const std::size_t count{ channel->subscribers.size() };

			try
			{
				for (std::size_t i{ 0 }; i < count; ++i)
				{
					SubscriberBase* base{ channel->subscribers[i].get() };
					if (!base->active) continue;

					auto* subscriber{ static_cast<Subscriber<E>*>(base) };
					subscriber->callback(event);
					++called;
				}
			}
			catch (...)
			{
				finish_dispatch(*channel, typeid(E));
				throw;
			}

			finish_dispatch(*channel, typeid(E));
			return called;
		}

		// Removes every subscription. Handlers already running keep executing.
		void clear()
		{
			std::lock_guard<std::recursive_mutex> lock{ mState->mutex };
			mState->channels.clear();
		}

		// Number of active subscriptions across all event types.
		std::size_t subscriber_count() const
		{
			std::lock_guard<std::recursive_mutex> lock{ mState->mutex };

			std::size_t count{ 0 };
			for (const auto& entry : mState->channels)
			{
				for (const auto& subscriber : entry.second->subscribers)
				{
					if (subscriber->active) ++count;
				}
			}
			return count;
		}

		bool empty() const { return subscriber_count() == 0; }

	private:
		// The caller must hold mState->mutex.
		void finish_dispatch(Channel& channel, std::type_index type)
		{
			--channel.depth;
			if (channel.depth != 0) return;

			channel.cleanup();
			if (!channel.subscribers.empty()) return;

			auto it = mState->channels.find(type);
			if (it != mState->channels.end() && it->second.get() == &channel)
				mState->channels.erase(it);
		}

		std::shared_ptr<State> mState{ std::make_shared<State>() };
	};
} // namespace wcore
