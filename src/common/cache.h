#pragma once

#include <ctime>

// A cached row: Put marks it dirty, Flush writes it back through OnFlush.
template <typename T>
class cache {
  public:
    cache() = default;

    virtual ~cache() = default;

    auto Get(bool update_time = true) -> T * {
        if (update_time) {
            m_lastUpdateTime = std::time(nullptr);
        }
        return &m_data;
    }

    void Put(const T *data, bool skip_query = false) {
        m_data = *data;
        m_lastUpdateTime = std::time(nullptr);

        if (!skip_query) {
            m_bNeedQuery = true;
        }
    }

    auto CheckFlushTimeout() const -> bool {
        return m_bNeedQuery && std::time(nullptr) - m_lastFlushTime > m_expireTime;
    }

    auto CheckTimeout() const -> bool {
        return std::time(nullptr) - m_lastUpdateTime > m_expireTime;
    }

    void Flush() {
        if (!m_bNeedQuery) {
            return;
        }

        OnFlush();

        m_bNeedQuery = false;
        m_lastFlushTime = std::time(nullptr);
    }

    virtual void OnFlush() = 0;

  protected:
    T m_data{};
    bool m_bNeedQuery{false};
    std::time_t m_expireTime{600};
    std::time_t m_lastUpdateTime{0};
    std::time_t m_lastFlushTime{std::time(nullptr)};
};
