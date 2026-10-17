#pragma once

// ── mlcpp::Queue<T> — C++ RAII wrapper for clib ml_queue ──────────────
//
// Typed, ownership-aware queue built on clib ml_queue_t.
// Uses std::unique_ptr<T> for clear ownership semantics.
// Internally adapts between void* (C ABI) and unique_ptr<T> (C++ RAII).

#include <memory>

#include "ml_queue.h"
#include "ml_pool.h"

namespace mlcpp {

template <typename T>
class Queue {
public:
    explicit Queue(int capacity, ml_pool_t *pool = nullptr)
        : q_(ml_queue_create(capacity, pool)) {}

    ~Queue() {
        // Drain remaining items before destroy so their destructors run
        if (q_) {
            void *item = nullptr;
            while (ml_queue_pop(q_, &item, 0) == 0 && item) {
                std::unique_ptr<T> owned(static_cast<T*>(item));
                (void)owned;  // destructor runs here
            }
            ml_queue_destroy(q_);
        }
    }

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    // ── Push (ownership transfers into queue) ─────────────────────────

    bool push(std::unique_ptr<T> item, int timeoutMs = -1) {
        if (!q_ || !item) return false;
        void *raw = static_cast<void*>(item.release());
        if (ml_queue_push(q_, raw, timeoutMs) != 0) {
            // Push failed: reclaim ownership and destroy
            std::unique_ptr<T> reclaim(static_cast<T*>(raw));
            return false;
        }
        return true;
    }

    // ── Pop (ownership transfers to caller) ───────────────────────────

    std::unique_ptr<T> pop(int timeoutMs = -1) {
        if (!q_) return nullptr;
        void *raw = nullptr;
        if (ml_queue_pop(q_, &raw, timeoutMs) != 0 || !raw)
            return nullptr;
        return std::unique_ptr<T>(static_cast<T*>(raw));
    }

    // ── Control ──────────────────────────────────────────────────────

    void interrupt() {
        if (q_) ml_queue_interrupt(q_);
    }

    void close() {
        if (q_) ml_queue_close(q_);
    }

    bool isClosed() const {
        return q_ ? ml_queue_is_closed(q_) : true;
    }

    int capacity() const { return q_ ? ml_queue_capacity(q_) : 0; }
    int count()    const { return q_ ? ml_queue_count(q_)    : 0; }
    bool empty()   const { return q_ ? ml_queue_empty(q_)    : true; }

    ml_queue_t* handle() { return q_; }

private:
    ml_queue_t *q_ = nullptr;
};

} // namespace mlcpp
