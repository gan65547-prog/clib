#pragma once

// ── mlcpp::Vision — C++ RAII wrapper for Vision SDK ml_vision ─────────
//
// Owns the ml_vision_t handle lifecycle: create → config → load → detect → destroy.
// Pool must outlive the Vision handle (caller ensures this).

#include <memory>

#include "ml_vision.h"

namespace mlcpp {

class Vision {
public:
    Vision(ml_vision_backend_t backend, ml_pool_t *pool)
        : v_(ml_vision_create(backend, pool)) {}

    ~Vision() {
        if (v_) ml_vision_destroy(v_);
    }

    Vision(const Vision&) = delete;
    Vision& operator=(const Vision&) = delete;

    Vision(Vision&& other) noexcept : v_(other.v_) {
        other.v_ = nullptr;
    }

    Vision& operator=(Vision&& other) noexcept {
        if (this != &other) {
            if (v_) ml_vision_destroy(v_);
            v_ = other.v_;
            other.v_ = nullptr;
        }
        return *this;
    }

    ml_vision_t* get() const { return v_; }
    explicit operator bool() const { return v_ != nullptr; }

private:
    ml_vision_t *v_ = nullptr;
};

} // namespace mlcpp
