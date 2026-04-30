// savestate.h - Lightweight serializer/deserializer used by save state.
#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace fcemu {

class Serializer {
public:
    explicit Serializer(std::vector<uint8_t>& out) : out_(out) {}
    template <typename T>
    void write(const T& v) {
        static_assert(std::is_trivially_copyable_v<T>, "POD only");
        const auto* p = reinterpret_cast<const uint8_t*>(&v);
        out_.insert(out_.end(), p, p + sizeof(T));
    }
    void write_bytes(const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        out_.insert(out_.end(), b, b + n);
    }
private:
    std::vector<uint8_t>& out_;
};

class Deserializer {
public:
    Deserializer(const uint8_t* data, size_t size) : data_(data), size_(size) {}
    template <typename T>
    void read(T& v) {
        static_assert(std::is_trivially_copyable_v<T>, "POD only");
        if (cur_ + sizeof(T) > size_) throw std::runtime_error("save state truncated");
        std::memcpy(&v, data_ + cur_, sizeof(T));
        cur_ += sizeof(T);
    }
    void read_bytes(void* p, size_t n) {
        if (cur_ + n > size_) throw std::runtime_error("save state truncated");
        std::memcpy(p, data_ + cur_, n);
        cur_ += n;
    }
    size_t remaining() const { return size_ - cur_; }
private:
    const uint8_t* data_;
    size_t size_;
    size_t cur_ = 0;
};

} // namespace fcemu
