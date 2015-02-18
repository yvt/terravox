#ifndef TEMPORARYBUFFER_H
#define TEMPORARYBUFFER_H

#include <memory>
#include <cassert>
#include <cstdint>

template <class T>
class TemporaryBuffer
{
    T *actual_storage_;
    T *storage_;
    std::size_t size_;
public:
    inline TemporaryBuffer(std::size_t size, std::size_t alignment = 64) :
        size_(size)
    {
        auto requiredSize = size + (alignment + sizeof(T) - 1) / sizeof(T);
        auto ret = std::get_temporary_buffer<T>(static_cast<std::ptrdiff_t>(requiredSize));
        actual_storage_ = ret.first;
        assert(static_cast<std::size_t>(ret.second) >= requiredSize);
        actual_storage_ = new T[requiredSize];

        storage_ = actual_storage_;
        storage_ = reinterpret_cast<T *>(reinterpret_cast<std::uintptr_t>(storage_) + alignment - 1 & ~(alignment - 1));
    }
    TemporaryBuffer(TemporaryBuffer &&) = delete;
    inline ~TemporaryBuffer()
    {
        std::return_temporary_buffer(actual_storage_);
    }

    inline T *get() const {
        return storage_;
    }
    inline operator T *() const {
        return storage_;
    }
    inline T *operator + (std::size_t i) const {
        return storage_ + i;
    }
    inline std::size_t size() const { return size_; }
    inline T &operator [] (std::size_t i) { return storage_[i]; }
    inline const T &operator [] (std::size_t i) const { return storage_[i]; }
    void operator = (const TemporaryBuffer &) = delete;

};

#endif // TEMPORARYBUFFER_H
