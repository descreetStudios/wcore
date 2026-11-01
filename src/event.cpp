#include <wcore/event.hpp>

#include <memory>
#include <mutex>
#include <typeindex>
#include <utility>
#include <vector>

namespace wcore
{
	void EventBus::Channel::cleanup()
	{
		std::erase_if(subscribers,
			[](const auto& subscriber) { return !subscriber->active; });
	}

	bool EventBus::State::remove(std::type_index type, u64 id)
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

	bool EventBus::State::contains(std::type_index type, u64 id) const
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

	EventBus::Subscription::~Subscription()
	{
		unsubscribe();
	}

	EventBus::Subscription::Subscription(Subscription&& other) noexcept
		: mState(std::move(other.mState)),
		mType(other.mType),
		mId(other.mId)
	{
		other.mId = 0;
	}

	EventBus::Subscription&
		EventBus::Subscription::operator=(Subscription&& other) noexcept
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

	void EventBus::Subscription::unsubscribe()
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

	bool EventBus::Subscription::active() const
	{
		if (mId == 0) return false;

		const std::shared_ptr<State> state{ mState.lock() };
		if (!state) return false;

		std::lock_guard<std::recursive_mutex> lock{ state->mutex };
		return state->contains(mType, mId);
	}

	void EventBus::clear()
	{
		std::lock_guard<std::recursive_mutex> lock{ mState->mutex };
		mState->channels.clear();
	}

	std::size_t EventBus::subscriber_count() const
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

	void EventBus::finish_dispatch(Channel& channel, std::type_index type)
	{
		--channel.depth;
		if (channel.depth != 0) return;

		channel.cleanup();
		if (!channel.subscribers.empty()) return;

		auto it = mState->channels.find(type);
		if (it != mState->channels.end() && it->second.get() == &channel)
			mState->channels.erase(it);
	}
}