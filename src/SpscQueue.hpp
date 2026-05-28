// AIPP_7B82C3F4_A8D1_4B29_BC3E_9F2C6E8A1D5B

#pragma once

#include "types_gen.hpp"
#include <atomic>
#include <cassert>
#include <cstring>
#include <memory>
#include <new>
#include <optional>
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

template<typename T>
class SpscQueue {
public:
  explicit SpscQueue(Capacity capacity);
  ~SpscQueue();

  SpscQueue(const SpscQueue&) = delete;
  SpscQueue& operator=(const SpscQueue&) = delete;
  SpscQueue(SpscQueue&&) = delete;
  SpscQueue& operator=(SpscQueue&&) = delete;

  bool try_enqueue(const T& value);
  std::optional<T> try_dequeue();

  Capacity capacity() const;
  bool is_empty() const;
  bool is_full() const;

private:
  // Extract the underlying value from the Capacity strong type
  static std::size_t capacity_value(Capacity cap) {
    return atlas_value_for(cap);
  }

  // Allocates and constructs a T in-place using placement new
  void construct_at(std::size_t index, const T& value);

  // Destroys a T in-place using explicit destructor call
  void destroy_at(std::size_t index);

  // Gets the pointer to the storage for element at index
  T* element_ptr(std::size_t index) const;

  std::unique_ptr<std::byte[]> buffer_;
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
    : capacity_(capacity_value(capacity)) {
  // Allocate buffer with capacity + 1 slots (one for sentinel)
  buffer_ = std::make_unique<std::byte[]>(
      (capacity_ + 1) * sizeof(T));
}

template<typename T>
SpscQueue<T>::~SpscQueue() {
  // Ensure all constructed elements are destructed
  auto write_pos = next_write_pos_.load(std::memory_order_relaxed);
  auto read_pos = next_read_pos_.load(std::memory_order_relaxed);

  while (read_pos != write_pos) {
    destroy_at(read_pos);
    read_pos = (read_pos + 1) % (capacity_ + 1);
  }
}

template<typename T>
bool SpscQueue<T>::try_enqueue(const T& value) {
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

  auto write = next_write_pos_.load(std::memory_order_relaxed);
  auto read = next_read_pos_.load(std::memory_order_acquire);

  auto next_write = (write + 1) % (capacity_ + 1);
  if (next_write == read) {
    return false; // Queue is full
  }

  try {
    construct_at(write, value);
  } catch (...) {
    throw; // Strong exception safety: queue unchanged
  }

  // Release semantics: ensure element is constructed before
  // consumer sees the updated write position
  next_write_pos_.store(next_write, std::memory_order_release);
  return true;
}

template<typename T>
std::optional<T> SpscQueue<T>::try_dequeue() {
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

  auto read = next_read_pos_.load(std::memory_order_relaxed);
  auto write = next_write_pos_.load(std::memory_order_acquire);

  if (read == write) {
    return std::nullopt; // Queue is empty
  }

  T* ptr = element_ptr(read);
  T result = std::move(*ptr);
  destroy_at(read);

  auto next_read = (read + 1) % (capacity_ + 1);
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
bool SpscQueue<T>::is_empty() const {
  return next_write_pos_.load(std::memory_order_relaxed) ==
         next_read_pos_.load(std::memory_order_relaxed);
}

template<typename T>
bool SpscQueue<T>::is_full() const {
  auto write = next_write_pos_.load(std::memory_order_relaxed);
  auto read = next_read_pos_.load(std::memory_order_relaxed);
  return (write + 1) % (capacity_ + 1) == read;
}

template<typename T>
void SpscQueue<T>::construct_at(std::size_t index, const T& value) {
  void* ptr = buffer_.get() + index * sizeof(T);
  new (ptr) T(value);
}

template<typename T>
void SpscQueue<T>::destroy_at(std::size_t index) {
  T* ptr = element_ptr(index);
  ptr->~T();
}

template<typename T>
T* SpscQueue<T>::element_ptr(std::size_t index) const {
  void* ptr = buffer_.get() + index * sizeof(T);
  return std::launder(reinterpret_cast<T*>(ptr));
}

} // namespace aipp
