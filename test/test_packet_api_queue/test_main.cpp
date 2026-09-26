// PacketApiQueue in src/mesh/PacketApiQueue.h isolates TFT delivery from external API consumers.
// Copies must survive mutations/releases of the external packet and allocator exhaustion. A stalled
// display must remain bounded, retaining important messages without draining the external client's queue.
// These tests guard against shared ownership, reordering, unbounded growth and loss on allocation failure.
#include "TestUtil.h"
#include "mesh/PacketApiQueue.h"
#include <cstdlib>
#include <unity.h>

struct TestPacket {
    unsigned id;
};

class TestPool
{
  public:
    bool fail = false;
    unsigned wait = 99;
    TestPacket result{};
    TestPacket *allocCopy(const TestPacket &packet, unsigned timeout)
    {
        wait = timeout;
        if (fail)
            return nullptr;
        result = packet;
        return &result;
    }
};

void setUp() {}
void tearDown() {}

void test_display_copy_survives_external_consumption()
{
    PacketApiQueue<TestPacket, 2> display;
    TestPool pool;
    TestPacket external{42};
    TEST_ASSERT_TRUE(display.isEmpty());
    TEST_ASSERT_TRUE(display.enqueue(external));
    TEST_ASSERT_FALSE(display.isEmpty());
    external.id = 0;
    TEST_ASSERT_EQUAL_UINT(42, display.dequeue(pool)->id);
    TEST_ASSERT_EQUAL_UINT(0, external.id);
    TEST_ASSERT_NULL(display.dequeue(pool));
    TEST_ASSERT_TRUE(display.isEmpty());
}

void test_full_queue_preserves_existing_packets_unless_priority_replaces_oldest()
{
    PacketApiQueue<TestPacket, 2> display;
    TestPool pool;
    TEST_ASSERT_TRUE(display.enqueue({1}));
    TEST_ASSERT_TRUE(display.enqueue({2}));
    TEST_ASSERT_FALSE(display.enqueue({3}, false));
    TEST_ASSERT_EQUAL_UINT(1, display.dequeue(pool)->id);
    TEST_ASSERT_EQUAL_UINT(2, display.dequeue(pool)->id);
    TEST_ASSERT_TRUE(display.enqueue({1}));
    TEST_ASSERT_TRUE(display.enqueue({2}));
    TEST_ASSERT_TRUE(display.enqueue({4}, true));
    TEST_ASSERT_EQUAL_UINT(2, display.dequeue(pool)->id);
    TEST_ASSERT_EQUAL_UINT(4, display.dequeue(pool)->id);
    TEST_ASSERT_NULL(display.dequeue(pool));
}

void test_allocation_failure_retains_packet_without_blocking()
{
    PacketApiQueue<TestPacket, 2> display;
    TestPool pool;
    display.enqueue({7});
    display.enqueue({8});
    pool.fail = true;
    TEST_ASSERT_NULL(display.dequeue(pool));
    TEST_ASSERT_EQUAL_UINT(0, pool.wait);
    TEST_ASSERT_FALSE(display.isEmpty());
    pool.fail = false;
    TEST_ASSERT_EQUAL_UINT(7, display.dequeue(pool)->id);
    TEST_ASSERT_EQUAL_UINT(8, display.dequeue(pool)->id);
}

void test_repeated_wrap_keeps_fifo_order()
{
    PacketApiQueue<TestPacket, 2> display;
    TestPool pool;
    for (unsigned i = 0; i < 100; ++i) {
        display.enqueue({i});
        display.enqueue({i + 1});
        TEST_ASSERT_EQUAL_UINT(i, display.dequeue(pool)->id);
        TEST_ASSERT_EQUAL_UINT(i + 1, display.dequeue(pool)->id);
        TEST_ASSERT_NULL(display.dequeue(pool));
    }
}

void setup()
{
    initializeTestEnvironment();
    UNITY_BEGIN();
    RUN_TEST(test_display_copy_survives_external_consumption);
    RUN_TEST(test_full_queue_preserves_existing_packets_unless_priority_replaces_oldest);
    RUN_TEST(test_allocation_failure_retains_packet_without_blocking);
    RUN_TEST(test_repeated_wrap_keeps_fifo_order);
    exit(UNITY_END());
}

void loop() {}
