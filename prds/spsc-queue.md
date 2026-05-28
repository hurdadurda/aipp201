# SPSC Queue Implementation PRD

## Problem Statement

The aipp project needs a lock-free, single-producer-single-consumer (SPSC) queue for efficient inter-thread communication. This queue must support fixed-size capacity, handle arbitrary element types, provide strong type safety through Atlas strong types, and minimize false sharing on modern multi-core systems.

## Solution

Implement a header-only `SpscQueue<T>` template class using a fixed-size ring buffer with a sentinel slot. The queue uses lock-free atomic operations with appropriate memory ordering, uninitialized storage with explicit object lifetime management, and cache-line-aware padding to prevent false sharing. The public API uses only Atlas strong types, and the implementation includes debug-build thread safety validation.

## User Stories

1. As a systems programmer, I want a lock-free SPSC queue so that I can communicate between threads with minimal overhead.

2. As a library user, I want to enqueue values into the queue so that a consumer thread can process them later.

3. As a library user, I want to dequeue values from the queue so that I can process work submitted by a producer.

4. As a library user, I want to query whether the queue is empty or full so that I can handle overflow conditions in my application logic.

5. As a library user, I want to know the capacity of the queue so that I can size my buffer appropriately.

6. As a library user, I want strong type safety at the API boundary so that I cannot accidentally pass incorrect types to the queue.

7. As a library user, I want to query queue state (empty/full/capacity) from either producer or consumer thread so that I can reason about queue status without synchronization penalties.

8. As a developer switching between producer threads, I want the queue to allow thread handoff so that different threads can produce to the same queue over time (with proper synchronization by the caller).

9. As a developer switching between consumer threads, I want the queue to allow thread handoff so that different threads can consume from the same queue over time (with proper synchronization by the caller).

10. As a library user, I want enqueue to fail gracefully when the queue is full so that I can handle backpressure without exceptions.

11. As a library user, I want dequeue to fail gracefully when the queue is empty so that I can handle starvation without exceptions.

12. As a library user, I want the queue to handle large element types efficiently so that I am not forced to store only pointers.

13. As a library user, I want the queue to provide strong exception safety on enqueue so that if element construction fails, my element is not consumed.

14. As a developer, I want the queue to detect concurrent access from multiple producers in debug builds so that I can catch threading bugs early.

15. As a developer, I want the queue to detect concurrent access from multiple consumers in debug builds so that I can catch threading bugs early.

16. As a performance-conscious user, I want cache-line padding on atomic positions so that producer and consumer do not contend on the same cache line.

17. As a performance-conscious user, I want minimal memory overhead so that the queue is suitable for embedded and performance-critical applications.

18. As a developer, I want unit tests that verify correct behavior on empty, partially-filled, and full queues so that I can trust the implementation.

19. As a developer, I want multi-threaded integration tests so that I can verify real producer-consumer workloads.

20. As a developer, I want property-based tests that verify invariants (total enqueued == total dequeued) so that I catch subtle race conditions.

## Implementation Decisions

### Modules

**SpscQueue Template Module**
- A single header-only class template `SpscQueue<T>` parameterized by element type `T`.
- Requires `T` to be move-constructible.
- Internal state: uninitialized byte buffer, atomic write position, atomic read position.
- No copying or moving of the queue itself; tied to specific producer and consumer threads over its lifetime.

**Capacity Strong Type**
- New Atlas strong type `Capacity` wrapping an unsigned integer.
- Defined in `types.atlas` alongside other project strong types.
- Represents the usable capacity of the queue (not including the sentinel slot).

**Testing Support**
- Test utilities in the existing `aipp::testing` namespace for thread-safe test coordination if needed.

### Public API Interface

**Constructor**
- `explicit SpscQueue(Capacity capacity)` — allocates internal buffer for `capacity + 1` slots (including sentinel).

**Producer Methods**
- `bool try_enqueue(const T& value)` — copies the value into the queue if space is available; returns true on success, false if full. Strong exception safety: if the copy constructor throws, queue state is unchanged and the exception propagates.

**Consumer Methods**
- `std::optional<T> try_dequeue()` — removes and returns the next value from the queue if available; returns `std::nullopt` if empty.

**Shared Query Methods (callable from either thread)**
- `Capacity capacity() const` — returns the usable capacity.
- `bool is_empty() const` — returns true if no elements are enqueued.
- `bool is_full() const` — returns true if the queue has no free slots.

**Deleted Methods**
- Copy constructor and assignment operator.
- Move constructor and assignment operator.
- (The queue is not movable or copyable; it is tied to specific threads.)

### Technical Decisions

**Ring Buffer with Sentinel Slot**
- Fixed-size circular array of size `capacity + 1`.
- One slot is always empty (the sentinel/guard slot).
- Full condition: `(next_write_pos + 1) % (capacity + 1) == next_read_pos`.
- Empty condition: `next_write_pos == next_read_pos`.

**Lock-Free Atomics**
- `std::atomic<size_t> next_write_pos` — only modified by the producer; read by the consumer to check full.
- `std::atomic<size_t> next_read_pos` — only modified by the consumer; read by the producer to check full.
- Memory ordering: `std::memory_order_relaxed` for most operations; `std::memory_order_release` on write operations, `std::memory_order_acquire` on reads of the opposite position.

**Cache-Line Padding**
- Both `next_write_pos` and `next_read_pos` are declared with `alignas(std::hardware_destructive_interference_size)` to ensure they occupy separate cache lines and prevent false sharing.

**Uninitialized Storage**
- Buffer allocated as `std::unique_ptr<std::byte[]>`.
- Elements constructed on-demand using placement-new in `try_enqueue`.
- Elements destructed explicitly in `try_dequeue` using the destructor call operator.
- Positions and byte offsets used to access the correct storage location.
- `std::launder` used when accessing constructed objects to satisfy strict-aliasing rules.

**Thread Safety Validation (Debug Only)**
- `std::atomic<std::thread::id> producer_thread_` — tracks the last thread ID that called `try_enqueue`.
- `std::atomic<std::thread::id> consumer_thread_` — tracks the last thread ID that called `try_dequeue`.
- In debug builds (guarded by `#ifndef NDEBUG`):
  - `try_enqueue` asserts that either `producer_thread_` is uninitialized or matches the current thread.
  - `try_dequeue` asserts that either `consumer_thread_` is uninitialized or matches the current thread.
  - Query methods do not check thread IDs.
- In release builds, these members are not compiled.

**File Organization**
- `src/SpscQueue.hpp` — header-only implementation of the template.
- `src/types.atlas` — definition of the `Capacity` strong type.
- CMake configuration in `src/CMakeLists.txt` to expose the header as part of the public API.

### Strong Type Usage

- All public methods use `Capacity` for size/capacity parameters.
- The element type `T` is the template parameter (not a strong type itself, since it is caller-provided).
- Return types use standard library types (`bool`, `std::optional<T>`) alongside strong types where semantically appropriate.

## Testing Decisions

### What Makes a Good Test

For a lock-free concurrent data structure, a good test:
- Exercises the external API (try_enqueue, try_dequeue, capacity, is_empty, is_full) rather than internal implementation details like atomic positions or memory ordering.
- Verifies correct behavior under various conditions: empty queue, partially-filled queue, completely full queue.
- For concurrent tests, uses real threads to verify producer-consumer interactions.
- Detects data races via ThreadSanitizer (TSan) in CI.
- Uses property-based testing to uncover subtle invariant violations that deterministic tests might miss.

### Modules to Test

**SpscQueue Implementation**
- Single-threaded correctness: sequences of enqueue/dequeue operations, boundary conditions.
- Multi-threaded stress: real producer and consumer threads operating concurrently.
- Property-based: RapidCheck-based tests that generate random sequences and verify queue invariants.
- Exception safety: verify strong exception safety when element copy constructor throws.

### Prior Art

- Existing `Foo_ut.cpp` provides a reference for test structure and DocTest integration.
- RapidCheck integration in the aipp project enables property-based testing (see `rc::doctest::check` examples).
- ThreadSanitizer is already part of the build pipeline (see CMakePresets for -asan variants).

## Out of Scope

- **Dynamic resizing**: The queue is fixed-size; resizing is out of scope.
- **Blocking variants**: `try_enqueue` and `try_dequeue` are non-blocking only. Blocking variants (e.g., wait for space or wait for element) are not included.
- **Custom allocators**: The queue uses the default allocator for its internal buffer; custom allocators are out of scope.
- **Move semantics for the queue itself**: The queue is neither copyable nor movable. Allowing moves would be complex and is not needed.
- **Intrusive queue variants**: The queue is non-intrusive; no intrusive variants are planned.
- **Benchmarking**: Performance benchmarks are not part of this PRD; they can be added later if needed.

## Further Notes

- The SPSC queue is a foundational primitive for lock-free systems. Once implemented and tested, it can serve as the basis for other concurrent data structures (e.g., MPMC queues, work-stealing queues).
- Debug-build thread ID validation is a low-cost safety mechanism; it catches common threading bugs early without overhead in production.
- The choice to take `const T&` (rather than `T&&`) in `try_enqueue` makes the API more ergonomic: callers retain ownership of their values on failure and can handle backpressure (e.g., enqueue to an overflow collection). The copy overhead is acceptable for most use cases.
- Cache-line padding using `std::hardware_destructive_interference_size` is a portable approach to false-sharing mitigation; fallback constants can be added if the standard constant is unavailable on some platforms.
- The implementation is intentionally simple and focused: no fancy optimizations, no platform-specific code, just correct lock-free semantics with strong types.
