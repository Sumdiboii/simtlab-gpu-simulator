#pragma once

#include <simtlab/isa/parser.hpp>
#include <simtlab/isa/warp.hpp>

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace simtlab
{

class ExecutionState
{
public:
    explicit ExecutionState(Program program)
        : program_(std::move(program)), terminated_(program_.instructions.empty())
    {
    }

    ExecutionState(const ExecutionState&) = default;
    ExecutionState& operator=(const ExecutionState&) = default;

    ExecutionState(ExecutionState&& other) noexcept
        : program_(std::move(other.program_)), pc_(other.pc_), warp_(std::move(other.warp_)),
          terminated_(other.terminated_)
    {
        other.resetMovedFrom();
    }

    ExecutionState& operator=(ExecutionState&& other) noexcept
    {
        if (this != &other)
        {
            program_ = std::move(other.program_);
            pc_ = other.pc_;
            warp_ = std::move(other.warp_);
            terminated_ = other.terminated_;
            other.resetMovedFrom();
        }
        return *this;
    }

    const Program& program() const noexcept { return program_; }
    std::size_t pc() const noexcept { return pc_; }
    Warp& warp() noexcept { return warp_; }
    const Warp& warp() const noexcept { return warp_; }
    bool terminated() const noexcept { return terminated_; }

    const Instruction& currentInstruction() const
    {
        if (terminated_ || pc_ >= program_.instructions.size())
            throw std::out_of_range("program counter is past the end of the program");
        return program_.instructions[pc_];
    }

    // Advance only the PC; callers handle instruction semantics separately.
    void advanceProgramCounter()
    {
        if (terminated_ || pc_ >= program_.instructions.size())
            throw std::out_of_range("cannot advance a terminated execution state");
        ++pc_;
        terminated_ = pc_ == program_.instructions.size();
    }

    void step();

private:
    void resetMovedFrom() noexcept
    {
        program_ = Program{};
        pc_ = 0;
        warp_ = Warp{};
        terminated_ = true;
    }

    Program program_;
    std::size_t pc_ = 0;
    Warp warp_{};
    bool terminated_ = false;
};

} // namespace simtlab
