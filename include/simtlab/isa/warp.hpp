#pragma once

#include <simtlab/isa/lane_state.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace simtlab
{

class ExecutionState;

inline constexpr std::size_t WARP_SIZE = 32;

struct Warp
{
    std::array<LaneState, WARP_SIZE> lanes{};

    Warp() = default;
    Warp(const Warp&) = default;
    Warp(Warp&&) noexcept = default;
    Warp& operator=(const Warp&) = default;
    Warp& operator=(Warp&&) noexcept = default;

    std::uint32_t activeMask() const noexcept { return active_mask_; }

private:
    friend class ExecutionState;
    std::uint32_t active_mask_ = ~std::uint32_t{0};
};

} // namespace simtlab
