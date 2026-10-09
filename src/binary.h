// SPDX-License-Identifier: MIT

// Bounds-checked reads for the NintendoWare sound formats, in the byte order that each file's byte order mark gives:
// little-endian on the Switch.

#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>

namespace hacsf2
{

// Invalid or unsupported input.
class FormatError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

// Reader over a byte span in either byte order. Out-of-bounds reads throw FormatError.
class Reader
{
public:
    Reader(std::span<const uint8_t> data, bool big_endian) : data_(data), big_endian_(big_endian)
    {
    }

    std::size_t Size() const
    {
        return data_.size();
    }

    bool BigEndian() const
    {
        return big_endian_;
    }

    std::span<const uint8_t> Sub(std::size_t off, std::size_t len) const
    {
        Check(off, len);

        return data_.subspan(off, len);
    }

    uint8_t U8(std::size_t off) const
    {
        Check(off, 1);

        return data_[off];
    }

    uint16_t U16(std::size_t off) const
    {
        Check(off, 2);

        const uint8_t* p = data_.data() + off;
        return big_endian_ ? static_cast<uint16_t>(p[0] << 8 | p[1]) : static_cast<uint16_t>(p[0] | p[1] << 8);
    }

    int16_t S16(std::size_t off) const
    {
        return static_cast<int16_t>(U16(off));
    }

    uint32_t U32(std::size_t off) const
    {
        Check(off, 4);

        const uint8_t* p = data_.data() + off;
        const uint32_t b0 = p[0], b1 = p[1], b2 = p[2], b3 = p[3];
        return big_endian_ ? b0 << 24 | b1 << 16 | b2 << 8 | b3 : b0 | b1 << 8 | b2 << 16 | b3 << 24;
    }

    int32_t S32(std::size_t off) const
    {
        return static_cast<int32_t>(U32(off));
    }

    float F32(std::size_t off) const
    {
        return std::bit_cast<float>(U32(off));
    }

    // Read a null-terminated string at `off`, up to `max_len` characters or the end of the data.
    std::string CString(std::size_t off, std::size_t max_len) const
    {
        std::string s;
        while (off < data_.size() && data_[off] && s.size() < max_len)
        {
            s.push_back(static_cast<char>(data_[off++]));
        }

        return s;
    }

    // Returns true if the 4 bytes at `off` are `m`.
    bool Magic(std::size_t off, const char* m) const
    {
        Check(off, 4);

        return std::memcmp(data_.data() + off, m, 4) == 0;
    }

private:
    void Check(std::size_t off, std::size_t len) const
    {
        if (off > data_.size() || len > data_.size() - off)
        {
            throw FormatError("read out of bounds at offset " + std::to_string(off));
        }
    }

    std::span<const uint8_t> data_;
    bool big_endian_;
};

// A NintendoWare reference: 16-bit type ID, 2 padding bytes and a signed 32-bit offset. The offset's base depends on
// the containing structure.
struct Reference
{
    uint16_t type = 0;
    int32_t offset = -1;
};

inline Reference ReadRef(const Reader& r, std::size_t off)
{
    return Reference{r.U16(off), r.S32(off + 4)};
}

} // namespace hacsf2
