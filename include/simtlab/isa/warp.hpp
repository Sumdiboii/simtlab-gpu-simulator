#pragma once

#include <simtlab/isa/lane_state.hpp>

#include <array>
#include <cstddef>

namespace simtlab
{

inline constexpr std::size_t WARP_SIZE = 32;

struct Warp
{
    std::array<LaneState, WARP_SIZE> lanes{};
};

} // namespace simtlab
