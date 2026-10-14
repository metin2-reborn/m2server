#pragma once

#include <cassert>

// Base of a class with one instance at a time: the object registers itself on construction and
// is reachable through instance() until it is destroyed.
template <typename T>
class singleton {
  public:
    singleton() {
        assert(instance_ == nullptr);

        instance_ = static_cast<T *>(this);
    }

    singleton(const singleton &) = delete;

    auto operator=(const singleton &) -> singleton & = delete;

    virtual ~singleton() {
        assert(instance_ != nullptr);

        instance_ = nullptr;
    }

    static auto instance() -> T & {
        assert(instance_ != nullptr);

        return *instance_;
    }

  private:
    inline static T *instance_ = nullptr;
};
