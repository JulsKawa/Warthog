#pragma once
#include "general/funds.hpp"
#include <cassert>
#include <cstdint>
class Writer;
class Reader;
Writer& operator<<(Writer&, CompactUInt);
class CompactUInt {
public:
    CompactUInt(uint16_t val)
        : val(val)
    {
    }
    CompactUInt(Reader& r);
    auto to_string() const { return Funds(*this).to_string(); }
    static CompactUInt compact(Funds);
    static consteval size_t byte_size(){return sizeof(val);}
    operator Funds() const
    {
        return uncompact();
    }
    Funds uncompact() const
    { // OK
        uint64_t e = (val & uint64_t(0xFC00u)) >> 10;
        uint64_t m = (val & uint64_t(0x03FFu)) + uint64_t(0x0400u);
        if (e < 10) {
            return Funds(m >> (10 - e));
        } else {
            return Funds(m << (e - 10));
        }
    }
    auto next() const
    {
        auto res(*this);
        res.val+=1;
        return res;
        assert(res.val != 0);
    }
    uint16_t value() const { return val; }

    // default comparison is correct even without uncompacting.
    auto operator<=>(const CompactUInt&) const = default;

private:
    uint16_t val;
};
