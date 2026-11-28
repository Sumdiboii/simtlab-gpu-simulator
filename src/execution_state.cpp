#include <simtlab/isa/execution_state.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace simtlab
{
namespace
{
std::size_t registerIndex(const Operand& operand)
{
    const auto* index = std::get_if<GprIndex>(&operand);
    if (index == nullptr || static_cast<unsigned int>(*index) >= GPR_COUNT)
        throw std::runtime_error("expected valid GPR operand");
    return static_cast<std::size_t>(*index);
}

}

void ExecutionState::step()
{
    const Instruction& instruction = currentInstruction();
    if (instruction.predicate) throw std::runtime_error("predicated execution is not implemented");

    const Opcode opcode = instruction.opcode;
    std::size_t operandCount = 3;
    bool immediateAllowed = false;
    bool fma = false;
    switch (opcode)
    {
    case Opcode::ADD:
    case Opcode::SUB:
    case Opcode::AND:
    case Opcode::OR:
    case Opcode::SHL:
    case Opcode::SHR: immediateAllowed = true; break;
    case Opcode::MUL: break;
    case Opcode::FADD:
    case Opcode::FMUL: break;
    case Opcode::FMA: fma = true; operandCount = 4; break;
    default: throw std::runtime_error("unsupported instruction in execution state");
    }
    if (instruction.operands.size() != operandCount)
        throw std::runtime_error("invalid operand count for instruction");

    const std::size_t destination = registerIndex(instruction.operands[0]);
    const std::size_t source1 = registerIndex(instruction.operands[1]);
    std::size_t source2 = 0;
    bool source2IsRegister = true;
    if (const auto* index = std::get_if<GprIndex>(&instruction.operands[2]))
    {
        source2 = static_cast<std::size_t>(*index);
        if (source2 >= GPR_COUNT) throw std::runtime_error("invalid source GPR index");
    }
    else if (immediateAllowed && std::holds_alternative<std::uint32_t>(instruction.operands[2]))
    {
        source2IsRegister = false;
    }
    else
    {
        throw std::runtime_error("invalid source operand for instruction");
    }
    std::size_t source3 = 0;
    if (fma) source3 = registerIndex(instruction.operands[3]);

    std::array<std::uint32_t, WARP_SIZE> results{};
    for (std::size_t laneIndex = 0; laneIndex < WARP_SIZE; ++laneIndex)
    {
        const LaneState& lane = warp_.lanes[laneIndex];
        const std::uint32_t left = lane.general_purpose_registers[source1];
        const std::uint32_t right = source2IsRegister
            ? lane.general_purpose_registers[source2]
            : std::get<std::uint32_t>(instruction.operands[2]);
        switch (opcode)
        {
        case Opcode::ADD: results[laneIndex] = left + right; break;
        case Opcode::SUB: results[laneIndex] = left - right; break;
        case Opcode::MUL:
            results[laneIndex] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(left) * right);
            break;
        case Opcode::AND: results[laneIndex] = left & right; break;
        case Opcode::OR: results[laneIndex] = left | right; break;
        case Opcode::SHL: results[laneIndex] = left << (right & 31u); break;
        case Opcode::SHR: results[laneIndex] = left >> (right & 31u); break;
        case Opcode::FADD:
        {
            const float a = std::bit_cast<float>(left);
            const float b = std::bit_cast<float>(right);
            results[laneIndex] = std::bit_cast<std::uint32_t>(static_cast<float>(a + b));
            break;
        }
        case Opcode::FMUL:
        {
            const float a = std::bit_cast<float>(left);
            const float b = std::bit_cast<float>(right);
            results[laneIndex] = std::bit_cast<std::uint32_t>(static_cast<float>(a * b));
            break;
        }
        case Opcode::FMA:
        {
            const float a = std::bit_cast<float>(left);
            const float b = std::bit_cast<float>(right);
            const float c = std::bit_cast<float>(lane.general_purpose_registers[source3]);
            results[laneIndex] = std::bit_cast<std::uint32_t>(std::fma(a, b, c));
            break;
        }
        default: throw std::runtime_error("unsupported instruction in execution state");
        }
    }

    for (std::size_t laneIndex = 0; laneIndex < WARP_SIZE; ++laneIndex)
        warp_.lanes[laneIndex].general_purpose_registers[destination] = results[laneIndex];
    advanceProgramCounter();
}

} // namespace simtlab
