#include "SpscQueue.hpp"
#include "testing/doctest.hpp"
#include <rapidcheck.h>
#include <rapidcheck/doctest.h>
#include <thread>
#include <vector>

namespace aipp::test {

// =====================================================================
// DocTest Unit Tests
// =====================================================================

TEST_SUITE("SpscQueue - Construction and Capacity") {
  TEST_CASE("Create queue with specified capacity") {
    aipp::SpscQueue<int> queue(aipp::Capacity(static_cast<std::size_t>(10)));
    CHECK(atlas_value_for(queue.capacity()) == 10);
    CHECK(queue.is_empty());
    CHECK(!queue.is_full());
  }

  TEST_CASE("Different capacities") {
    aipp::SpscQueue<int> q1(aipp::Capacity(static_cast<std::size_t>(1)));
    CHECK(atlas_value_for(q1.capacity()) == 1);

    aipp::SpscQueue<int> q100(
        aipp::Capacity(static_cast<std::size_t>(100)));
    CHECK(atlas_value_for(q100.capacity()) == 100);
  }
}

TEST_SUITE("SpscQueue - Basic Enqueue/Dequeue") {
  TEST_CASE("Enqueue to empty queue succeeds") {
    aipp::SpscQueue<int> queue(
        aipp::Capacity(static_cast<std::size_t>(10)));
    CHECK(queue.try_enqueue(42));
    CHECK(!queue.is_empty());
  }

  TEST_CASE("Dequeue from queue with one element") {
    aipp::SpscQueue<int> queue(
        aipp::Capacity(static_cast<std::size_t>(10)));
    queue.try_enqueue(42);
    auto result = queue.try_dequeue();
    REQUIRE(result.has_value());
    CHECK(result.value() == 42);
    CHECK(queue.is_empty());
  }

  TEST_CASE("Enqueue and dequeue multiple elements in order") {
    aipp::SpscQueue<int> queue(
        aipp::Capacity(static_cast<std::size_t>(10)));
    for (int i = 1; i <= 5; ++i) {
      CHECK(queue.try_enqueue(i));
    }

    for (int i = 1; i <= 5; ++i) {
      auto result = queue.try_dequeue();
      REQUIRE(result.has_value());
      CHECK(result.value() == i);
    }
    CHECK(queue.is_empty());
  }

  TEST_CASE("Round-robin enqueue/dequeue pattern") {
    aipp::SpscQueue<int> queue(aipp::Capacity(static_cast<std::size_t>(3)));
    CHECK(queue.try_enqueue(1));
    CHECK(queue.try_enqueue(2));
    auto v1 = queue.try_dequeue();
    REQUIRE(v1.has_value());
    CHECK(v1.value() == 1);

    CHECK(queue.try_enqueue(3));
    auto v2 = queue.try_dequeue();
    REQUIRE(v2.has_value());
    CHECK(v2.value() == 2);

    auto v3 = queue.try_dequeue();
    REQUIRE(v3.has_value());
    CHECK(v3.value() == 3);
    CHECK(queue.is_empty());
  }
}

TEST_SUITE("SpscQueue - Empty Queue Behavior") {
  TEST_CASE("Dequeue from empty queue returns nullopt") {
    aipp::SpscQueue<int> queue(
        aipp::Capacity(static_cast<std::size_t>(10)));
    auto result = queue.try_dequeue();
    CHECK(!result.has_value());
  }

  TEST_CASE("Multiple dequeues from empty queue") {
    aipp::SpscQueue<int> queue(
        aipp::Capacity(static_cast<std::size_t>(10)));
    CHECK(!queue.try_dequeue().has_value());
    CHECK(!queue.try_dequeue().has_value());
    CHECK(!queue.try_dequeue().has_value());
  }

  TEST_CASE("Dequeue becomes available after enqueue") {
    aipp::SpscQueue<int> queue(
        aipp::Capacity(static_cast<std::size_t>(10)));
    CHECK(!queue.try_dequeue().has_value());
    queue.try_enqueue(99);
    auto result = queue.try_dequeue();
    REQUIRE(result.has_value());
    CHECK(result.value() == 99);
  }
}

TEST_SUITE("SpscQueue - Full Queue Behavior") {
  TEST_CASE("Enqueue to full queue fails") {
    aipp::SpscQueue<int> queue(aipp::Capacity(static_cast<std::size_t>(2)));
    CHECK(queue.try_enqueue(1));
    CHECK(queue.try_enqueue(2));
    CHECK(!queue.try_enqueue(3));
  }

  TEST_CASE("Queue is full when capacity is reached") {
    aipp::SpscQueue<int> queue(aipp::Capacity(static_cast<std::size_t>(3)));
    CHECK(!queue.is_full());
    queue.try_enqueue(1);
    CHECK(!queue.is_full());
    queue.try_enqueue(2);
    CHECK(!queue.is_full());
    queue.try_enqueue(3);
    CHECK(queue.is_full());
  }

  TEST_CASE("After dequeue, space becomes available") {
    aipp::SpscQueue<int> queue(aipp::Capacity(static_cast<std::size_t>(2)));
    queue.try_enqueue(1);
    queue.try_enqueue(2);
    CHECK(queue.is_full());
    queue.try_dequeue();
    CHECK(!queue.is_full());
    CHECK(queue.try_enqueue(3));
  }

  TEST_CASE("Cannot enqueue when full, but value is not consumed") {
    aipp::SpscQueue<int> queue(aipp::Capacity(static_cast<std::size_t>(1)));
    queue.try_enqueue(42);
    int original_value = 99;
    CHECK(!queue.try_enqueue(original_value));
    CHECK(original_value == 99); // Value not consumed
  }
}

TEST_SUITE("SpscQueue - Large Element Types") {
  struct LargeType {
    std::array<char, 1024> data;
    int value;

    LargeType() : value(0) { data.fill('\0'); }
    explicit LargeType(int v) : value(v) {
      data.fill(static_cast<char>(v % 256));
    }
  };

  TEST_CASE("Enqueue and dequeue large struct") {
    aipp::SpscQueue<LargeType> queue(aipp::Capacity(static_cast<std::size_t>(5)));
    LargeType original(42);
    CHECK(queue.try_enqueue(original));
    auto result = queue.try_dequeue();
    REQUIRE(result.has_value());
    CHECK(result.value().value == 42);
  }

  TEST_CASE("Multiple large structs maintain order") {
    aipp::SpscQueue<LargeType> queue(
        aipp::Capacity(static_cast<std::size_t>(10)));
    for (int i = 0; i < 5; ++i) {
      CHECK(queue.try_enqueue(LargeType(i)));
    }
    for (int i = 0; i < 5; ++i) {
      auto result = queue.try_dequeue();
      REQUIRE(result.has_value());
      CHECK(result.value().value == i);
    }
  }
}

TEST_SUITE("SpscQueue - Exception Safety") {
  struct ThrowingType {
    int value;
    bool should_throw;

    ThrowingType(int v, bool throw_on_copy = false)
        : value(v), should_throw(throw_on_copy) {}

    ThrowingType(const ThrowingType& other)
        : value(other.value), should_throw(other.should_throw) {
      if (should_throw) {
        throw std::runtime_error("Copy constructor throws");
      }
    }

    ThrowingType& operator=(const ThrowingType& other) = delete;
  };

  TEST_CASE("Enqueue with throwing copy constructor") {
    aipp::SpscQueue<ThrowingType> queue(
        aipp::Capacity(static_cast<std::size_t>(5)));

    ThrowingType safe_value(42, false);
    CHECK(queue.try_enqueue(safe_value));

    ThrowingType throwing_value(99, true);
    CHECK_THROWS(queue.try_enqueue(throwing_value));

    // Queue should still be functional
    CHECK(queue.try_enqueue(ThrowingType(100, false)));

    auto result = queue.try_dequeue();
    REQUIRE(result.has_value());
    CHECK(result.value().value == 42);
  }

  TEST_CASE("After failed enqueue, queue state unchanged") {
    aipp::SpscQueue<ThrowingType> queue(
        aipp::Capacity(static_cast<std::size_t>(3)));
    queue.try_enqueue(ThrowingType(1, false));
    queue.try_enqueue(ThrowingType(2, false));

    ThrowingType should_fail(999, true);
    CHECK_THROWS(queue.try_enqueue(should_fail));
    CHECK(!queue.is_full()); // Still room for one more (sentinel)

    // Original elements still there
    auto v1 = queue.try_dequeue();
    REQUIRE(v1.has_value());
    CHECK(v1.value().value == 1);
    auto v2 = queue.try_dequeue();
    REQUIRE(v2.has_value());
    CHECK(v2.value().value == 2);
  }
}

TEST_SUITE("SpscQueue - Capacity One Edge Case") {
  TEST_CASE("Capacity 1 queue (minimal size)") {
    aipp::SpscQueue<int> queue(aipp::Capacity(static_cast<std::size_t>(1)));
    CHECK(queue.is_empty());
    CHECK(!queue.is_full());

    CHECK(queue.try_enqueue(42));
    CHECK(queue.is_full());
    CHECK(!queue.is_empty());

    CHECK(!queue.try_enqueue(99)); // Cannot enqueue while full

    auto result = queue.try_dequeue();
    REQUIRE(result.has_value());
    CHECK(result.value() == 42);
    CHECK(queue.is_empty());
    CHECK(!queue.is_full());
  }
}

// =====================================================================
// Multi-threaded Integration Tests (non-property-based)
// =====================================================================

TEST_SUITE("SpscQueue - Multi-threaded Operations") {
  TEST_CASE("Producer and consumer run concurrently") {
    aipp::SpscQueue<int> queue(aipp::Capacity(static_cast<std::size_t>(20)));
    constexpr int ITEMS = 100;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    std::thread producer([&]() {
      for (int i = 0; i < ITEMS; ++i) {
        while (!queue.try_enqueue(i)) {
          std::this_thread::yield();
        }
        produced.store(i + 1, std::memory_order_release);
      }
    });

    std::thread consumer([&]() {
      for (int i = 0; i < ITEMS; ++i) {
        std::optional<int> value;
        while (!(value = queue.try_dequeue()).has_value()) {
          std::this_thread::yield();
        }
        CHECK(value.value() == i);
        consumed.store(i + 1, std::memory_order_release);
      }
    });

    producer.join();
    consumer.join();

    CHECK(produced.load() == ITEMS);
    CHECK(consumed.load() == ITEMS);
    CHECK(queue.is_empty());
  }

  TEST_CASE("High-speed producer-consumer") {
    aipp::SpscQueue<int> queue(aipp::Capacity(static_cast<std::size_t>(10)));
    constexpr int ITEMS = 1000;
    std::vector<int> dequeued;

    std::thread producer([&]() {
      for (int i = 0; i < ITEMS; ++i) {
        while (!queue.try_enqueue(i)) {
          // Busy wait
        }
      }
    });

    std::thread consumer([&]() {
      while (static_cast<int>(dequeued.size()) < ITEMS) {
        auto value = queue.try_dequeue();
        if (value.has_value()) {
          dequeued.push_back(value.value());
        }
      }
    });

    producer.join();
    consumer.join();

    CHECK(dequeued.size() == static_cast<std::size_t>(ITEMS));
    for (int i = 0; i < ITEMS; ++i) {
      CHECK(dequeued[static_cast<std::size_t>(i)] == i);
    }
  }

  TEST_CASE("Producer waits for space (backpressure)") {
    aipp::SpscQueue<int> queue(aipp::Capacity(static_cast<std::size_t>(5)));
    std::atomic<int> enqueued{0};
    std::atomic<int> dequeued{0};

    std::thread producer([&]() {
      for (int i = 0; i < 20; ++i) {
        while (!queue.try_enqueue(i)) {
          std::this_thread::yield();
        }
        enqueued.fetch_add(1, std::memory_order_release);
      }
    });

    std::thread consumer([&]() {
      // Consume slowly
      for (int i = 0; i < 20; ++i) {
        std::optional<int> value;
        while (!(value = queue.try_dequeue()).has_value()) {
          std::this_thread::yield();
        }
        std::this_thread::sleep_for(
            std::chrono::microseconds(10));
        dequeued.fetch_add(1, std::memory_order_release);
      }
    });

    producer.join();
    consumer.join();

    CHECK(enqueued.load() == 20);
    CHECK(dequeued.load() == 20);
  }
}

// =====================================================================
// RapidCheck Property-Based Tests
// =====================================================================

TEST_SUITE("SpscQueue - Property-Based Tests") {
  TEST_CASE("Invariant: Total enqueued equals total dequeued") {
    rc::doctest::check(
        "FIFO sequences",
        [](const std::vector<int>& values) {
          aipp::SpscQueue<int> queue(
              aipp::Capacity(static_cast<std::size_t>(100)));

          for (int v : values) {
            while (!queue.try_enqueue(v)) {
              // Ensure all values are enqueued
            }
          }

          std::vector<int> dequeued;
          while (auto v = queue.try_dequeue()) {
            dequeued.push_back(v.value());
          }

          return (dequeued.size() == values.size()) &&
                 (dequeued == values);
        });
  }

  TEST_CASE("Invariant: FIFO ordering is maintained") {
    rc::doctest::check("Values dequeued in same order as enqueued",
                       [](const std::vector<int>& values) {
                         aipp::SpscQueue<int> queue(
                             aipp::Capacity(static_cast<std::size_t>(
                                 values.size() + 1)));

                         for (int v : values) {
                           if (!queue.try_enqueue(v)) {
                             return false;
                           }
                         }

                         for (int expected : values) {
                           auto result = queue.try_dequeue();
                           if (!result.has_value() ||
                               result.value() != expected) {
                             return false;
                           }
                         }

                         return queue.is_empty();
                       });
  }

  TEST_CASE("Invariant: Queue never exceeds capacity") {
    rc::doctest::check("Queue respects capacity bounds",
                       [](std::size_t cap) {
                         if (cap < 1 || cap > 1000) {
                           return true;
                         }

                         aipp::SpscQueue<int> queue{aipp::Capacity(cap)};

                         std::size_t enqueued = 0;
                         for (std::size_t i = 0;
                              i < cap + 100; ++i) {
                           if (queue.try_enqueue(
                                   static_cast<int>(i))) {
                             enqueued++;
                           }
                         }

                         if (enqueued > cap) {
                           return false;
                         }

                         std::size_t dequeued = 0;
                         while (queue.try_dequeue()
                                    .has_value()) {
                           dequeued++;
                         }

                         return dequeued == enqueued;
                       });
  }

  TEST_CASE("Invariant: No data loss in interleaved operations") {
    rc::doctest::check(
        "All enqueued values are dequeued exactly once",
        [](const std::vector<int>& values) {
          if (values.empty() || values.size() > 500) {
            return true;
          }

          aipp::SpscQueue<int> queue(aipp::Capacity(
              static_cast<std::size_t>(values.size() + 1)));

          for (int v : values) {
            while (!queue.try_enqueue(v)) {
              // Retry until successful
            }
          }

          std::vector<int> dequeued;
          for (std::size_t i = 0; i < values.size(); ++i) {
            auto v = queue.try_dequeue();
            if (!v.has_value()) {
              return false;
            }
            dequeued.push_back(v.value());
          }

          return queue.is_empty() && (dequeued == values);
        });
  }
}

} // namespace aipp::test
