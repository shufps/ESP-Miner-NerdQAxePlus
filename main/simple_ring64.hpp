#pragma once
#include <cstdint>
#include <cstddef>

template <std::size_t N>
class SimpleRing64
{
public:
    // Insert only if the value is not already present; returns true if inserted.
    bool insert_if_absent(uint64_t v)
    {
        if (contains(v)) { return false; }
        push(v);
        return true;
    }

    // Push a value; overwrites the oldest element when the buffer is full.
    void push(uint64_t v)
    {
        m_buf[m_head] = v;
        m_head = (m_head + 1) % N;
        if (m_size < N) { ++m_size; }
    }

    // Linear membership check: O(N)
    bool contains(uint64_t v) const
    {
        std::size_t start = (m_head + N - m_size) % N; // oldest element
        for (std::size_t i = 0; i < m_size; ++i) {
            if (m_buf[(start + i) % N] == v) { return true; }
        }
        return false;
    }

    void clear() { m_head = 0; m_size = 0; }
    std::size_t size() const { return m_size; }
    static constexpr std::size_t capacity() { return N; }

private:
    uint64_t m_buf[N] = {};
    std::size_t m_head = 0;   // next write position (oldest slot)
    std::size_t m_size = 0;   // number of valid elements
};
