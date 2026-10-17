#pragma once

// ── mlcpp::Pool — C++ RAII wrapper for clib ml_pool ────────────────────
//
// Stack-allocated pool with RAII lifecycle. create/destroy are called
// automatically. Use .get() to pass the pool pointer to C APIs.

#include "ml_pool.h"

namespace mlcpp {

class Pool {
public:
    Pool() { ml_pool_create(&pool_, 0); }
    ~Pool() { ml_pool_destroy(&pool_); }

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    Pool(Pool&& other) noexcept : pool_(other.pool_) {
        other.pool_ = ml_pool_t{};  // moved-from pool is zeroed
    }

    Pool& operator=(Pool&& other) noexcept {
        if (this != &other) {
            ml_pool_destroy(&pool_);
            pool_ = other.pool_;
            other.pool_ = ml_pool_t{};
        }
        return *this;
    }

    ml_pool_t* get() { return &pool_; }

private:
    ml_pool_t pool_{};
};

} // namespace mlcpp
