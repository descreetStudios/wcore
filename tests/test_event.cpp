#include <wcore/test.hpp>
#include <wcore/event.hpp>

#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace wcore;
using namespace test;

namespace
{
	struct PingEvent
	{
		int value{ 0 };
	};

	struct PongEvent
	{
		int value{ 0 };
	};
}

TEST(event_bus_publish_without_subscribers)
{
	EventBus bus;

	CHECK_EQ(bus.publish(PingEvent{ 1 }), 0u);
	CHECK(bus.empty());
	CHECK_EQ(bus.subscriber_count(), 0u);
}

TEST(event_bus_default_subscription_is_inactive)
{
	EventBus::Subscription sub;

	CHECK(!sub.active());
	CHECK(!static_cast<bool>(sub));
}

TEST(event_bus_subscribe_receives_event)
{
	EventBus bus;
	int value{ 0 };

	auto sub = bus.subscribe<PingEvent>([&](const PingEvent& event)
	{
		value = event.value;
	});

	CHECK(sub.active());
	CHECK_EQ(bus.subscriber_count(), 1u);
	CHECK_EQ(bus.publish(PingEvent{ 7 }), 1u);
	CHECK_EQ(value, 7);
}

TEST(event_bus_only_invokes_matching_type)
{
	EventBus bus;
	int pings{ 0 };
	int pongs{ 0 };

	auto pingSub = bus.subscribe<PingEvent>([&](const PingEvent&) { ++pings; });
	auto pongSub = bus.subscribe<PongEvent>([&](const PongEvent&) { ++pongs; });

	CHECK_EQ(bus.publish(PingEvent{ 1 }), 1u);
	CHECK_EQ(pings, 1);
	CHECK_EQ(pongs, 0);

	CHECK_EQ(bus.publish(PongEvent{ 2 }), 1u);
	CHECK_EQ(pings, 1);
	CHECK_EQ(pongs, 1);
}

TEST(event_bus_invokes_handlers_in_subscription_order)
{
	EventBus bus;
	std::vector<int> order;

	auto first = bus.subscribe<int>([&](const int&) { order.push_back(1); });
	auto second = bus.subscribe<int>([&](const int&) { order.push_back(2); });
	auto third = bus.subscribe<int>([&](const int&) { order.push_back(3); });

	CHECK_EQ(bus.publish(0), 3u);
	REQUIRE(order.size() == 3u);
	CHECK_EQ(order[0], 1);
	CHECK_EQ(order[1], 2);
	CHECK_EQ(order[2], 3);
}

TEST(event_bus_subscription_destroy_unsubscribes)
{
	EventBus bus;
	int calls{ 0 };

	{
		auto sub = bus.subscribe<int>([&](const int&) { ++calls; });
		CHECK_EQ(bus.publish(0), 1u);
	}

	CHECK_EQ(bus.publish(0), 0u);
	CHECK_EQ(calls, 1);
	CHECK(bus.empty());
}

TEST(event_bus_explicit_unsubscribe)
{
	EventBus bus;
	int calls{ 0 };

	auto sub = bus.subscribe<int>([&](const int&) { ++calls; });
	sub.unsubscribe();

	CHECK(!sub.active());
	CHECK(!static_cast<bool>(sub));
	CHECK_EQ(bus.publish(0), 0u);
	CHECK_EQ(calls, 0);
	CHECK(bus.empty());

	sub.unsubscribe(); // idempotent
}

TEST(event_bus_move_subscription_transfers_ownership)
{
	EventBus bus;
	int calls{ 0 };

	auto first = bus.subscribe<int>([&](const int&) { ++calls; });
	auto second = std::move(first);

	CHECK(!first.active());
	CHECK(second.active());
	CHECK_EQ(bus.publish(0), 1u);
	CHECK_EQ(calls, 1);

	second.unsubscribe();
	CHECK_EQ(bus.publish(0), 0u);
}

TEST(event_bus_unsubscribe_self_during_dispatch)
{
	EventBus bus;
	EventBus::Subscription sub;
	int calls{ 0 };

	sub = bus.subscribe<int>([&](const int&)
	{
		++calls;
		sub.unsubscribe();
	});

	CHECK_EQ(bus.publish(0), 1u);
	CHECK_EQ(calls, 1);
	CHECK(!sub.active());
	CHECK_EQ(bus.publish(0), 0u);
	CHECK_EQ(calls, 1);
}

TEST(event_bus_unsubscribe_other_during_dispatch)
{
	EventBus bus;
	EventBus::Subscription second;
	int calls{ 0 };

	auto first = bus.subscribe<int>([&](const int&)
	{
		++calls;
		second.unsubscribe();
	});

	second = bus.subscribe<int>([&](const int&) { ++calls; });

	CHECK_EQ(bus.publish(0), 1u);
	CHECK_EQ(calls, 1);
	CHECK(!second.active());
}

TEST(event_bus_subscribe_during_dispatch_applies_to_later_publish)
{
	EventBus bus;
	EventBus::Subscription added;
	int calls{ 0 };

	auto first = bus.subscribe<int>([&](const int&)
	{
		++calls;
		if (!added)
			added = bus.subscribe<int>([&](const int&) { ++calls; });
	});

	CHECK_EQ(bus.publish(0), 1u);
	CHECK_EQ(calls, 1);
	CHECK(added.active());

	CHECK_EQ(bus.publish(0), 2u);
	CHECK_EQ(calls, 3);
}

TEST(event_bus_supports_nested_publish)
{
	EventBus bus;
	int calls{ 0 };

	auto first = bus.subscribe<int>([&](const int& value)
	{
		++calls;
		if (value == 0) bus.publish(1);
	});

	auto second = bus.subscribe<int>([&](const int&) { ++calls; });

	CHECK_EQ(bus.publish(0), 2u);
	CHECK_EQ(calls, 4);
}

TEST(event_bus_clear_removes_every_subscription)
{
	EventBus bus;
	int calls{ 0 };

	auto intSub = bus.subscribe<int>([&](const int&) { ++calls; });
	auto stringSub = bus.subscribe<std::string>([&](const std::string&) { ++calls; });
	CHECK_EQ(bus.subscriber_count(), 2u);

	bus.clear();

	CHECK(bus.empty());
	CHECK(!intSub.active());
	CHECK(!stringSub.active());
	CHECK_EQ(bus.publish(1), 0u);
	CHECK_EQ(bus.publish(std::string("event")), 0u);
	CHECK_EQ(calls, 0);
}

TEST(event_bus_clear_during_dispatch_finishes_current_dispatch)
{
	EventBus bus;
	int calls{ 0 };

	auto first = bus.subscribe<int>([&](const int&)
	{
		++calls;
		bus.clear();
	});

	auto second = bus.subscribe<int>([&](const int&) { ++calls; });

	CHECK_EQ(bus.publish(0), 2u);
	CHECK_EQ(calls, 2);
	CHECK(!first.active());
	CHECK(!second.active());
	CHECK_EQ(bus.publish(0), 0u);
}

TEST(event_bus_handler_exception_restores_dispatch_state)
{
	EventBus bus;
	int calls{ 0 };

	auto first = bus.subscribe<int>([&](const int&)
	{
		++calls;
		if (calls == 1) throw std::runtime_error("boom");
	});

	auto second = bus.subscribe<int>([&](const int&) { ++calls; });

	bool threw{ false };
	try
	{
		bus.publish(0);
	}
	catch (const std::runtime_error&)
	{
		threw = true;
	}

	CHECK(threw);
	CHECK_EQ(calls, 1);
	CHECK_EQ(bus.publish(0), 2u);
	CHECK_EQ(calls, 3);
}

TEST(event_bus_subscription_can_outlive_bus)
{
	EventBus::Subscription sub;

	{
		EventBus bus;
		sub = bus.subscribe<int>([](const int&) {});
		CHECK(sub.active());
	}

	CHECK(!sub.active());
	sub.unsubscribe(); // no-op
}

TEST(event_bus_publishes_from_multiple_threads)
{
	EventBus bus;
	std::atomic<int> calls{ 0 };

	auto sub = bus.subscribe<int>([&](const int&) { calls.fetch_add(1); });

	constexpr int threadCount{ 4 };
	constexpr int publishesPerThread{ 100 };
	std::vector<std::thread> workers;
	workers.reserve(threadCount);

	for (int i{ 0 }; i < threadCount; ++i)
	{
		workers.emplace_back([&]()
		{
			for (int j{ 0 }; j < publishesPerThread; ++j)
				bus.publish(0);
		});
	}

	for (auto& worker : workers)
		worker.join();

	CHECK_EQ(calls.load(), threadCount * publishesPerThread);
	CHECK_EQ(bus.subscriber_count(), 1u);
}
