#pragma once

#include <simtlab/isa/register.hpp>

#include <cstdint>
#include <string>
#include <variant>

namespace simtlab
{
struct MemoryOperand
{
    GprIndex base;
    std::variant<std::monostate, std::int32_t, GprIndex> offset;
};

struct LabelOperand
{
    std::string name;
};

enum class MemoryWidth : std::uint8_t
{
    BYTE = 1, HALFWORD = 2, WORD = 4
};

enum class SpecialRegister
{
    LANEID, WARPID, TID_X, TID_Y, TID_Z,
    CTAID_X, CTAID_Y, CTAID_Z,
    NTID_X, NTID_Y, NTID_Z,
    GRIDID_X, GRIDID_Y, GRIDID_Z
};

using Operand = std::variant<
    GprIndex,
    PredicateIndex,
    std::int32_t,
    std::uint32_t,
    float,
    MemoryOperand,
    LabelOperand,
    MemoryWidth,
    SpecialRegister
>;

} // namespace simtlab
