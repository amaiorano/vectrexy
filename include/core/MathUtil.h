#pragma once

namespace MathUtil {
    class AverageValue {
    public:
        void Reset() {
            m_sum = {};
            m_count = {};
        }
        void Add(float v) {
            m_sum += v;
            ++m_count;
        }
        float Sum() const { return m_sum; }
        size_t Count() const { return m_count; }
        float Average() const { return m_count == 0 ? 0 : m_sum / m_count; }
        float AverageAndReset() {
            auto result = Average();
            Reset();
            return result;
        }

    private:
        float m_sum{};
        size_t m_count{};
    };
} // namespace MathUtil
