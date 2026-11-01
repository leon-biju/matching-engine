#pragma once

#include <atomic>
#include <bit>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace matching::pipeline {

// The cache line size assumed for padding. It is fixed at 64 bytes rather than taken from
// std::hardware_destructive_interference_size, which GCC warns about because its value can change
// between compiler versions and flags.
inline constexpr std::size_t cache_line_size = 64;

// Waits before retrying a full or empty queue. It pauses the CPU for the first few tries, then
// yields the thread, so a waiting stage cannot starve the others when there are fewer cores than
// threads.
class Backoff {
  public:
    void wait() {
        if (spins_ < pause_limit) {
            ++spins_;
            pause();
        } else {
            std::this_thread::yield();
        }
    }

  private:
    static constexpr int pause_limit = 64;

    static void pause() {
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#elif defined(__aarch64__)
        asm volatile("yield");
#endif
    }

    int spins_ = 0;
};

// A bounded, lock-free queue for exactly one producer thread and one consumer thread.
//
// The producer owns tail_ and the consumer owns head_. Both only ever increase, and an index's
// slot is index & (capacity - 1), so the capacity must be a power of two. The queue is empty when
// head_ == tail_ and full when tail_ - head_ == capacity.
//
// The producer writes a slot, then publishes it by storing tail_ with release ordering. The
// consumer loads tail_ with acquire ordering before reading the slot, so it sees the write. Freeing
// a slot works the same way in the other direction through head_. So everything a thread wrote
// before a push is visible to the other thread after the matching pop, and the same holds for a
// pop and the push that later reuses its slot.
//
// Each thread's index sits on its own cache line, so the two threads do not take a line from each
// other on every operation. Each thread also keeps a cached copy of the other thread's index on
// its own line, and only reloads the real one when the copy says the queue is full or empty.
template <typename T> class SpscQueue {
  public:
    // Throws std::invalid_argument unless capacity is a power of two.
    explicit SpscQueue(std::size_t capacity) : slots_(capacity), mask_(capacity - 1) {
        if (!std::has_single_bit(capacity)) {
            throw std::invalid_argument("SPSC queue capacity must be a power of two");
        }
    }

    SpscQueue(const SpscQueue&) = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;

    std::size_t capacity() const { return slots_.size(); }

    // Producer only. Adds value and returns true, or returns false if the queue is full.
    bool try_push(const T& value) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail - cached_head_ == slots_.size()) {
            cached_head_ = head_.load(std::memory_order_acquire);
            if (tail - cached_head_ == slots_.size()) {
                return false;
            }
        }
        slots_[tail & mask_] = value;
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    // Consumer only. Removes and returns the oldest value, or returns nothing if the queue is
    // empty.
    std::optional<T> try_pop() {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        if (head == cached_tail_) {
            cached_tail_ = tail_.load(std::memory_order_acquire);
            if (head == cached_tail_) {
                return std::nullopt;
            }
        }
        std::optional<T> value(std::move(slots_[head & mask_]));
        head_.store(head + 1, std::memory_order_release);
        return value;
    }

    // Producer only. Adds value, waiting with a Backoff while the queue is full.
    void push(const T& value) {
        Backoff backoff;
        while (!try_push(value)) {
            backoff.wait();
        }
    }

    // Consumer only. Removes and returns the oldest value, waiting with a Backoff while the queue
    // is empty.
    T pop() {
        Backoff backoff;
        while (true) {
            if (std::optional<T> value = try_pop()) {
                return std::move(*value);
            }
            backoff.wait();
        }
    }

  private:
    // Read by both threads, written only by the constructor.
    std::vector<T> slots_;
    std::size_t mask_;

    // Written only by the consumer.
    alignas(cache_line_size) std::atomic<std::size_t> head_{0};
    std::size_t cached_tail_ = 0;

    // Written only by the producer.
    alignas(cache_line_size) std::atomic<std::size_t> tail_{0};
    std::size_t cached_head_ = 0;
};

} // namespace matching::pipeline
