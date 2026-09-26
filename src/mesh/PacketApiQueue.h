#pragma once

#include "concurrency/LockGuard.h"
#include <array>
#include <cstddef>

// Keep display copies outside the shared packet pools until the display consumes them.
template <typename T, size_t Capacity> class PacketApiQueue
{
    static_assert(Capacity > 0);
    std::array<T, Capacity> entries{};
    size_t head = 0;
    size_t count = 0;
    concurrency::Lock lock;

  public:
    bool isEmpty()
    {
        concurrency::LockGuard guard(&lock);
        return count == 0;
    }

    bool enqueue(const T &value, bool replaceOldest = true)
    {
        concurrency::LockGuard guard(&lock);
        if (count == Capacity) {
            if (!replaceOldest)
                return false;
            head = (head + 1) % Capacity;
            --count;
        }
        entries[(head + count) % Capacity] = value;
        ++count;
        return true;
    }

    template <typename Pool> T *dequeue(Pool &pool)
    {
        concurrency::LockGuard guard(&lock);
        if (!count)
            return nullptr;
        T *copy = pool.allocCopy(entries[head], 0);
        if (copy) {
            head = (head + 1) % Capacity;
            --count;
        }
        return copy;
    }
};
