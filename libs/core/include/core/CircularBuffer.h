#pragma once

#include <algorithm>
#include <cassert>
#include <vector>

template <typename T>
class CircularBuffer {
public:
    using ElemType = T;

    CircularBuffer(size_t maxSize = 0) { Init(maxSize); }

    void Init(size_t maxSize) {
        m_buffer.resize(maxSize);
        Clear();
    }

    // Clears all values in the buffer such that UsedSize()==0, and FreeSize()==TotalSize()
    void Clear() {
        m_begin = m_buffer.empty() ? nullptr : &m_buffer.front();
        m_front = m_back = m_begin;
        m_end = m_buffer.empty() ? nullptr : &m_buffer.back() + 1;
        m_wrapped = 0;
    }

    // Total number of elements that can be added to the buffer
    size_t TotalSize() const { return m_buffer.size(); }

    // Number of elements in the buffer
    size_t UsedSize() const {
        if (m_wrapped == 0) // front is behind back
        {
            return m_back - m_front;
        } else { // front is ahead of back
            return (m_end - m_front) + (m_back - m_begin);
        }
    }

    // Number of elements that can be added to the buffer before it's full
    size_t FreeSize() const { return TotalSize() - UsedSize(); }

    // Returns true if there are no elements in the buffer
    bool Empty() const { return UsedSize() == 0; }

    // Returns true if the buffer is full
    bool Full() const { return FreeSize() == 0; }

    // Attempts to push numValues from source into buffer; will not go past the front pointer.
    // Returns number of values actually pushed.
    size_t PushBack(const T* source, size_t numValues) {
        assert(m_wrapped < 2);
        size_t numValuesActuallyWritten = 0;

        if (m_wrapped == 0) // front <= back
        {
            assert(m_front <= m_back);
            const size_t roomLeft = m_end - m_back;
            const size_t numValuesForFirstWrite = std::min(numValues, roomLeft);

            std::copy_n(source, numValuesForFirstWrite, m_back);
            m_back += numValuesForFirstWrite;
            numValuesActuallyWritten += numValuesForFirstWrite;
            assert(m_back <= m_end);

            if (m_back == m_end) {
                m_back = m_begin;
                ++m_wrapped;
            }

            // If we've written all we have to, bail; otherwise, we still have more to write, so
            // update our inputs and fall through into m_wrapped == 1 condition below
            if (numValuesForFirstWrite == numValues) {
                return numValuesActuallyWritten;
            } else {
                source += numValuesForFirstWrite;
                numValues -= numValuesForFirstWrite;
            }
        }

        // Note: NOT "else if" here on purpose
        if (m_wrapped == 1) // front >= back
        {
            assert(m_front >= m_back);

            // Write as much as we can; but we can't go past the front pointer
            size_t roomLeft = m_front - m_back;
            const size_t numValuesToWrite = std::min(numValues, roomLeft);

            std::copy_n(source, numValuesToWrite, m_back);
            m_back += numValuesToWrite;
            numValuesActuallyWritten += numValuesToWrite;
            assert(m_back <= m_front);
        }

        return numValuesActuallyWritten;
    }

    // Push a single element to the back of the buffer.
    // Returns number of values actually pushed (0 or 1).
    size_t PushBack(const T& value) { return PushBack(&value, 1); }

    // Pushes back numValues, removing values from front if full
    void PushBackMoveFront(T* source, size_t numValues) {
        //@TODO: make this more efficient by popping multiple from front
        const size_t numActuallyPushed = PushBack(source, numValues);
        const size_t numNotPushed = numValues - numActuallyPushed;
        for (size_t i = 0; i < numNotPushed; ++i) {
            T dummy;
            PopFront(dummy);
        }
        const size_t numExtraPushed = PushBack(source + numActuallyPushed, numNotPushed);

        (void)numExtraPushed;
        assert(numExtraPushed == numNotPushed);
        assert(numActuallyPushed + numExtraPushed == numValues);
    }

    // Pushes single element to the back of the buffer, removing a value from the front if full.
    void PushBackMoveFront(T& value) { PushBackMoveFront(&value, 1); }

    // Attempts to pop numValues worth of data from the buffer into dest.
    // Returns how many values actually popped
    size_t PopFront(T* dest, size_t numValues) {
        //@TODO: Implement optimal version without constantly calling PopFront
        size_t numValuesActuallyRead = 0;
        for (size_t i = 0; i < numValues; ++i) {
            if (PopFront(dest[i])) {
                ++numValuesActuallyRead;
            } else {
                break;
            }
        }
        return numValuesActuallyRead;
    }

    // Pops a single value from the front of the buffer.
    // Returns how many values actually popped (0 or 1).
    size_t PopFront(T& value) {
        // Pop front, then increment front

        if (m_wrapped == 0) // front <= back
        {
            assert(m_front <= m_back);
            if (m_front == m_back)
                return 0;
            value = *m_front;
            IncFront();
            return 1;
        } else // front >= back
        {
            assert(m_front >= m_back);
            value = *m_front;
            IncFront();
            return 1;
        }
    }

    // Pops a single value from the back of the buffer.
    // Returns how many values actually popped (0 or 1).
    size_t PopBack(T& value) {
        // Decrement back, then pop back
        if (m_wrapped == 0) // front <= back
        {
            assert(m_front <= m_back);
            if (m_back == m_front)
                return 0;
            DecBack();
            value = *m_back;
            return 1;
        } else { // front >= back
            assert(m_front >= m_back);
            DecBack();
            value = *m_back;
            return 1;
        }
    }

    // Copies up to numValues worth of values from the back of the queue in "forward" order without
    // affecting the buffer. You must ensure that dest is at least as large as numValues. Returns
    // how many values actually copied in dest.
    size_t PeekBack(T* dest, size_t numValues) {
        //@TODO: Implement peekback without popping and pushing
        size_t i = 0;
        for (; i < numValues; ++i) {
            if (PopBack(dest[i]) == 0)
                break;
        }

        std::reverse(dest, dest + i);

        PushBack(dest, i);
        return i;
    }

    // Copies a single element from the back into 'value' if possible.
    // Returns number of values actually copied (0 or 1).
    size_t PeekBack(T& value) { return PeekBack(&value, 1); }

private:
    void IncBack() {
        if (++m_back == m_end) {
            m_back = m_begin;
            ++m_wrapped;
            assert(m_wrapped == 1);
        }
    }

    void DecBack() {
        if (m_back == m_begin) {
            m_back = m_end;
            --m_wrapped;
            assert(m_wrapped == 0);
        }
        --m_back;
    }

    void IncFront() {
        if (++m_front == m_end) {
            m_front = m_begin;
            --m_wrapped;
            assert(m_wrapped == 0);
        }
    }

    std::vector<T> m_buffer;
    T* m_begin;
    T* m_end;
    T* m_front;
    T* m_back;
    int m_wrapped;
};
