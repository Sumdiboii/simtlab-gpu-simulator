#pragma once

#include <simtlab/isa/parser.hpp>
#include <simtlab/isa/warp.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace simtlab
{

class ExecutionState
{
public:
    // Copy construction snapshots state; move construction transfers it and empties the source.
    // Assignment is disabled so an existing execution cannot replace its retired-lane history.
    explicit ExecutionState(Program program)
        : program_(std::move(program))
    {
        if (program_.instructions.empty()) warp_.active_mask_ = 0;
    }

    ExecutionState(const ExecutionState&) = default;
    ExecutionState& operator=(const ExecutionState&) = delete;

    ExecutionState(ExecutionState&& other) noexcept
        : program_(std::move(other.program_)), pc_(other.pc_), warp_(std::move(other.warp_))
    {
        other.resetMovedFrom();
    }

    ExecutionState& operator=(ExecutionState&&) = delete;

    const Program& program() const noexcept { return program_; }
    std::size_t pc() const noexcept { return pc_; }
    std::array<LaneState, WARP_SIZE>& lanes() noexcept { return warp_.lanes; }
    const std::array<LaneState, WARP_SIZE>& lanes() const noexcept { return warp_.lanes; }
    std::uint32_t activeMask() const noexcept { return warp_.active_mask_; }
    bool terminated() const noexcept { return warp_.active_mask_ == 0; }

    const Instruction& currentInstruction() const;

    // Advance only the PC; callers handle instruction semantics separately.
    void advanceProgramCounter();

    void step();

private:
    void resetMovedFrom() noexcept
    {
        program_ = Program{};
        pc_ = 0;
        warp_ = Warp{};
        warp_.active_mask_ = 0;
    }

    Program program_;
    std::size_t pc_ = 0;
    Warp warp_{};
    void executeInstruction(const Instruction& instruction);
    void advanceProgramCounterUnchecked();
};

} // namespace simtlab
