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

			void cleanup();
		};

		struct State
		{
			std::recursive_mutex mutex;
			std::unordered_map<std::type_index, std::shared_ptr<Channel>> channels;
			u64 nextId{ 1 };

			bool remove(std::type_index type, u64 id);

			bool contains(std::type_index type, u64 id) const;
		};

	public:
		class Subscription
		{
		public:
			Subscription() noexcept = default;
			~Subscription();

			Subscription(const Subscription&) = delete;
			Subscription& operator=(const Subscription&) = delete;

			Subscription(Subscription&& other) noexcept;
			Subscription& operator=(Subscription&& other) noexcept;

			void unsubscribe();

			bool active() const;

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

		void clear();

		std::size_t subscriber_count() const;

		bool empty() const { return subscriber_count() == 0; }

	private:
		void finish_dispatch(Channel& channel, std::type_index type);

		std::shared_ptr<State> mState{ std::make_shared<State>() };
	};
}
