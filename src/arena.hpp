#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>

class ArenaAllocator
{
public:
    inline explicit ArenaAllocator(size_t bytes)
        : m_size(bytes), m_buffer(static_cast<std::byte *>(malloc(m_size))), m_offset(m_buffer)
    {
    }

    template <typename T>
    inline T *alloc()
    {
        T *object = new (m_offset) T{};
        m_offset += sizeof(T);
        return object;
    }

    inline ArenaAllocator(const ArenaAllocator &other) = delete;

    inline ArenaAllocator &operator=(const ArenaAllocator &other) = delete;

    inline ~ArenaAllocator()
    {
        free(m_buffer);
    }

private:
    size_t m_size;
    std::byte *m_buffer;
    std::byte *m_offset;
};