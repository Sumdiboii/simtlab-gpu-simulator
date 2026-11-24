#pragma once

#include <simtlab/isa/register.hpp>

#include <array>
#include <cstdint>

namespace simtlab
{

struct LaneState
{
    std::array<std::uint32_t, GPR_COUNT> general_purpose_registers{};
    std::array<bool, PREDICATE_COUNT> predicate_registers{};
};

} // namespace simtlab
