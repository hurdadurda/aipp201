// AIPP_7B82C3F4_A8D1_4B29_BC3E_9F2C6E8A1D5B

#pragma once

#include "types_gen.hpp"
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <thread>

// Fallback for std::hardware_destructive_interference_size if not available
#if __cplusplus >= 202002L && \
    defined(__cpp_lib_hardware_interference_size)
#define AIPP_CACHE_LINE_SIZE std::hardware_destructive_interference_size
#else
// AIPP_CACHE_LINE_SIZE is defined by CMake based on hardware detection
// Default to 64 if not provided
#ifndef AIPP_CACHE_LINE_SIZE
#define AIPP_CACHE_LINE_SIZE 64
#endif
#endif

namespace aipp {

/// Single-producer, single-consumer lock-free queue.
/// Producer and consumer must run on different threads.
/// Producer calls try_enqueue() and producer_side_is_full().
/// Consumer calls try_dequeue() and consumer_side_is_empty().
///
/// Memory ordering: release on write ensures construction happens-before
/// consumer's acquire on read. Release on read position tells producer the slot is reusable.
///
/// Exception Safety: Basic. try_enqueue(const T&) may throw from T::T(const T&);
/// the queue remains valid and enqueue can be retried. try_enqueue(T&&) and
/// try_dequeue() do not throw. T must be nothrow-move-constructible and
/// nothrow-destructible.
///
/// Note: Thread affinity is validated only in debug builds.
/// Release builds assume the SPSC contract is honored by the caller.
template<typename T>
class SpscQueue {
public:
  static_assert(std::is_nothrow_move_constructible_v<T>,
                "T must be nothrow-move-constructible; "
                "required for std::optional<T> exception safety");
  static_assert(std::is_nothrow_destructible_v<T>,
                "T must have a nothrow destructor");

  explicit SpscQueue(Capacity capacity);
  ~SpscQueue() noexcept;

  SpscQueue(const SpscQueue&) = delete;
  SpscQueue& operator=(const SpscQueue&) = delete;
  SpscQueue(SpscQueue&&) = delete;
  SpscQueue& operator=(SpscQueue&&) = delete;

  [[nodiscard]] EnqueueResult try_enqueue(const T& value);
  [[nodiscard]] EnqueueResult try_enqueue(T&& value);
  [[nodiscard]] std::optional<T> try_dequeue();

  Capacity capacity() const;

  /// Returns whether this queue appeared full on the last snapshot.
  /// No synchronization with the consumer—result may be stale.
  [[nodiscard]] ProducerSideIsFull producer_side_is_full() const;

  /// Returns whether this queue appeared empty on the last snapshot.
  /// No synchronization with the producer—result may be stale.
  [[nodiscard]] ConsumerSideIsEmpty consumer_side_is_empty() const;

private:
  // Extract the underlying value from the Capacity strong type
  static std::size_t capacity_value(Capacity cap) {
    return atlas_value_for(cap);
  }

  // Unified enqueue implementation for both lvalue and rvalue
  template<typename U>
  EnqueueResult try_enqueue_impl(U&& value);

  // Allocates and constructs a T in-place using placement new
  template<typename U>
  void construct_at(std::size_t index, U&& value);

  // Destroys a T in-place using explicit destructor call
  void destroy_at(std::size_t index);

  // Gets the byte pointer for storage at index, respecting alignment
  std::byte* byte_ptr(std::size_t index) const;

  // Gets the typed pointer to the storage for element at index
  T* element_ptr(std::size_t index) const;

  // Calculates the next index with wraparound
  std::size_t next_index(std::size_t current) const noexcept;

  // Returns true if the queue is full given write and read positions
  bool is_full_at(std::size_t write_idx, std::size_t read_idx) const noexcept;

  // Validates that only one thread is currently a producer
  void validate_producer_thread();

  // Validates that only one thread is currently a consumer
  void validate_consumer_thread();

  std::unique_ptr<std::byte[], void(*)(void*)> buffer_;
  std::size_t capacity_;

  // Padded to prevent false sharing between producer and consumer
  alignas(AIPP_CACHE_LINE_SIZE)
      std::atomic<std::size_t> next_write_pos_{0};

  alignas(AIPP_CACHE_LINE_SIZE)
      std::atomic<std::size_t> next_read_pos_{0};

#ifndef NDEBUG
  alignas(AIPP_CACHE_LINE_SIZE)
      std::atomic<std::thread::id> producer_thread_;

  alignas(AIPP_CACHE_LINE_SIZE)
      std::atomic<std::thread::id> consumer_thread_;
#endif
};

// Implementation

template<typename T>
SpscQueue<T>::SpscQueue(Capacity capacity)
    : buffer_(nullptr, [](void* p) { std::free(p); }),
      capacity_(capacity_value(capacity)) {
  std::size_t cap = capacity_;

  if (cap == 0) {
    throw std::invalid_argument("SpscQueue capacity must be > 0");
  }

  // HIGH-002: Check for integer overflow, accounting for sentinel slot
  if (cap > std::numeric_limits<std::size_t>::max() / sizeof(T) - 1) {
    throw std::overflow_error("Capacity too large");
  }

  std::size_t alignment = alignof(T);
  std::size_t size = (cap + 1) * sizeof(T);
  void* ptr = std::aligned_alloc(alignment, size);
  if (!ptr) {
    throw std::bad_alloc();
  }
  buffer_.reset(static_cast<std::byte*>(ptr));
}

template<typename T>
SpscQueue<T>::~SpscQueue() noexcept {
  // Destructor is not racing; other threads are assumed dead. Relaxed is safe and optimal.
  auto write_pos = next_write_pos_.load(std::memory_order_relaxed);
  auto read_pos = next_read_pos_.load(std::memory_order_relaxed);

  while (read_pos != write_pos) {
    destroy_at(read_pos);
    read_pos = next_index(read_pos);
  }
}

template<typename T>
void SpscQueue<T>::validate_producer_thread() {
#ifndef NDEBUG
  auto current_thread = std::this_thread::get_id();
  auto expected = std::thread::id();
  if (!producer_thread_.compare_exchange_weak(
          expected, current_thread,
          std::memory_order_relaxed)) {
    assert(producer_thread_.load(std::memory_order_relaxed) ==
           current_thread);
  }
#endif
}

template<typename T>
void SpscQueue<T>::validate_consumer_thread() {
#ifndef NDEBUG
  auto current_thread = std::this_thread::get_id();
  auto expected = std::thread::id();
  if (!consumer_thread_.compare_exchange_weak(
          expected, current_thread,
          std::memory_order_relaxed)) {
    assert(consumer_thread_.load(std::memory_order_relaxed) ==
           current_thread);
  }
#endif
}

template<typename T>
std::size_t SpscQueue<T>::next_index(std::size_t current) const noexcept {
  return (current + 1) % (capacity_ + 1);
}

template<typename T>
bool SpscQueue<T>::is_full_at(std::size_t write_idx,
                               std::size_t read_idx) const noexcept {
  return next_index(write_idx) == read_idx;
}

template<typename T>
EnqueueResult SpscQueue<T>::try_enqueue(const T& value) {
  return try_enqueue_impl(value);
}

template<typename T>
EnqueueResult SpscQueue<T>::try_enqueue(T&& value) {
  return try_enqueue_impl(std::forward<T>(value));
}

template<typename T>
template<typename U>
EnqueueResult SpscQueue<T>::try_enqueue_impl(U&& value) {
  validate_producer_thread();

  auto write = next_write_pos_.load(std::memory_order_relaxed);
  auto read = next_read_pos_.load(std::memory_order_acquire);

  auto next_write = next_index(write);
  if (is_full_at(write, read)) {
    return aipp::EnqueueResult(false); // Queue is full
  }

  construct_at(write, std::forward<U>(value));

  // Release semantics: ensure element is constructed before
  // consumer sees the updated write position
  next_write_pos_.store(next_write, std::memory_order_release);
  return aipp::EnqueueResult(true);
}

template<typename T>
std::optional<T> SpscQueue<T>::try_dequeue() {
  validate_consumer_thread();

  auto read = next_read_pos_.load(std::memory_order_relaxed);
  auto write = next_write_pos_.load(std::memory_order_acquire);

  if (read == write) {
    return std::nullopt; // Queue is empty
  }

  T* ptr = element_ptr(read);
  T result = std::move(*ptr);
  destroy_at(read);

  auto next_read = next_index(read);
  // Release semantics: ensure element is destructed before
  // producer sees the updated read position
  next_read_pos_.store(next_read, std::memory_order_release);
  return result;
}

template<typename T>
Capacity SpscQueue<T>::capacity() const {
  return aipp::Capacity(capacity_);
}

template<typename T>
ProducerSideIsFull SpscQueue<T>::producer_side_is_full() const {
  auto write = next_write_pos_.load(std::memory_order_relaxed);
  auto read = next_read_pos_.load(std::memory_order_relaxed);
  bool is_full = is_full_at(write, read);
  return aipp::ProducerSideIsFull(is_full);
}

template<typename T>
ConsumerSideIsEmpty SpscQueue<T>::consumer_side_is_empty() const {
  bool is_empty = next_write_pos_.load(std::memory_order_relaxed) ==
                  next_read_pos_.load(std::memory_order_relaxed);
  return aipp::ConsumerSideIsEmpty(is_empty);
}

template<typename T>
template<typename U>
void SpscQueue<T>::construct_at(std::size_t index, U&& value) {
  std::byte* ptr = byte_ptr(index);
  new (ptr) T(std::forward<U>(value));
}

template<typename T>
void SpscQueue<T>::destroy_at(std::size_t index) {
  T* ptr = element_ptr(index);
  ptr->~T();
}

template<typename T>
std::byte* SpscQueue<T>::byte_ptr(std::size_t index) const {
  return buffer_.get() + index * sizeof(T);
}

template<typename T>
T* SpscQueue<T>::element_ptr(std::size_t index) const {
  std::byte* ptr = byte_ptr(index);
  return std::launder(reinterpret_cast<T*>(ptr));
}

} // namespace aipp
