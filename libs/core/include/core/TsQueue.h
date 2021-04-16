#pragma once

#include <mutex>
#include <optional>
#include <queue>

// Thread-safe queue
template <typename T>
class TsQueue {
public:
    template <typename U>
    void push(U&& v) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::forward<U>(v));
    }

    std::optional<T> pop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty())
            return {};
        auto v = m_queue.front();
        m_queue.pop();
        return v;
    }

private:
    std::mutex m_mutex;
    std::queue<T> m_queue;
};
