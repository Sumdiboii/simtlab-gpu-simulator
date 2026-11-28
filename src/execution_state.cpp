#include <simtlab/isa/execution_state.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <variant>

static_assert(sizeof(float) == sizeof(std::uint32_t),
              "SimtLab requires 32-bit float for IEEE-754 binary32 execution");
static_assert(std::numeric_limits<float>::is_iec559,
              "SimtLab requires IEC 559 / IEEE-754 floating-point semantics");

namespace simtlab
{
namespace
{
std::string executionDiagnostic(const Instruction* instruction, const char* message)
{
    std::string result(message);
    if (instruction != nullptr && instruction->source_location)
    {
        const std::string suffix = " at " + std::to_string(instruction->source_location->line) + ":"
            + std::to_string(instruction->source_location->column);
        if (!result.ends_with(suffix)) result += suffix;
    }
    else
    {
        constexpr char fallback[] = " (source location unavailable for this execution state or programmatically constructed instruction)";
        if (!result.ends_with(fallback)) result += fallback;
    }
    return result;
}

std::size_t registerIndex(const Operand& operand)
{
    const auto* index = std::get_if<GprIndex>(&operand);
    if (index == nullptr || static_cast<unsigned int>(*index) >= GPR_COUNT)
        throw std::runtime_error("expected valid GPR operand");
    return static_cast<std::size_t>(*index);
}

std::size_t predicateIndex(const Operand& operand)
{
    const auto* index = std::get_if<PredicateIndex>(&operand);
    if (index == nullptr || static_cast<unsigned int>(*index) >= PREDICATE_COUNT)
        throw std::runtime_error("expected valid predicate operand");
    return static_cast<std::size_t>(*index);
}
}

const Instruction& ExecutionState::currentInstruction() const
{
    const Instruction* current = pc_ < program_.instructions.size()
        ? &program_.instructions[pc_] : nullptr;
    if (terminated())
        throw std::out_of_range(executionDiagnostic(current, "program counter is past the end of the program"));
    if (current == nullptr)
        throw std::runtime_error(executionDiagnostic(nullptr, "program ended while lanes remain active").c_str());
    return *current;
}

void ExecutionState::advanceProgramCounterUnchecked()
{
    if (pc_ >= program_.instructions.size())
        throw std::out_of_range("program counter is past the end of the program");
    if (pc_ + 1 == program_.instructions.size() && warp_.active_mask_ != 0)
        throw std::runtime_error("program ends while lanes remain active");
    ++pc_;
}

void ExecutionState::advanceProgramCounter()
{
    const Instruction* current = pc_ < program_.instructions.size()
        ? &program_.instructions[pc_] : nullptr;
    try
    {
        if (terminated()) throw std::out_of_range("cannot advance a terminated execution state");
        advanceProgramCounterUnchecked();
    }
    catch (const std::out_of_range& error)
    {
        throw std::out_of_range(executionDiagnostic(current, error.what()));
    }
    catch (const std::exception& error)
    {
        throw std::runtime_error(executionDiagnostic(current, error.what()));
    }
}

void ExecutionState::step()
{
    const Instruction* instruction = nullptr;
    try
    {
        if (pc_ < program_.instructions.size()) instruction = &program_.instructions[pc_];
        instruction = &currentInstruction();
        executeInstruction(*instruction);
    }
    catch (const std::out_of_range& error)
    {
        throw std::out_of_range(executionDiagnostic(instruction, error.what()));
    }
    catch (const std::exception& error)
    {
        throw std::runtime_error(executionDiagnostic(instruction, error.what()));
    }
}

void ExecutionState::executeInstruction(const Instruction& instruction)
{
    const Opcode opcode = instruction.opcode;
    std::size_t operandCount = 3;
    bool immediateAllowed = false;
    bool hasDestinationPredicate = false;
    bool isFloatComparison = false;
    bool isFma = false;
    bool isExit = false;
    switch (opcode)
    {
    case Opcode::ADD:
    case Opcode::SUB:
    case Opcode::AND:
    case Opcode::OR:
    case Opcode::SHL:
    case Opcode::SHR: immediateAllowed = true; break;
    case Opcode::MUL:
    case Opcode::FADD:
    case Opcode::FMUL: break;
    case Opcode::FMA: isFma = true; operandCount = 4; break;
    case Opcode::SETP_EQ:
    case Opcode::SETP_NE:
    case Opcode::SETP_LT:
    case Opcode::SETP_LE:
    case Opcode::SETP_GT:
    case Opcode::SETP_GE:
        hasDestinationPredicate = true;
        break;
    case Opcode::SETP_FEQ:
    case Opcode::SETP_FNE:
    case Opcode::SETP_FLT:
    case Opcode::SETP_FLE:
    case Opcode::SETP_FGT:
    case Opcode::SETP_FGE:
        hasDestinationPredicate = true;
        isFloatComparison = true;
        break;
    case Opcode::EXIT: isExit = true; operandCount = 0; break;
    default: throw std::runtime_error("unsupported instruction in execution state");
    }
    if (instruction.operands.size() != operandCount)
        throw std::runtime_error("invalid operand count for instruction");

    std::size_t predicateDestination = 0;
    std::size_t destination = 0;
    std::size_t source1 = 0;
    std::size_t source2 = 0;
    std::size_t source3 = 0;
    bool source2IsRegister = true;
    if (hasDestinationPredicate)
    {
        predicateDestination = predicateIndex(instruction.operands[0]);
        source1 = registerIndex(instruction.operands[1]);
        source2 = registerIndex(instruction.operands[2]);
    }
    else if (!isExit)
    {
        destination = registerIndex(instruction.operands[0]);
        source1 = registerIndex(instruction.operands[1]);
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
        if (isFma) source3 = registerIndex(instruction.operands[3]);
    }

    std::size_t instructionPredicate = 0;
    if (instruction.predicate)
    {
        if (static_cast<unsigned int>(instruction.predicate->index) >= PREDICATE_COUNT)
            throw std::runtime_error("invalid instruction predicate index");
        instructionPredicate = static_cast<std::size_t>(instruction.predicate->index);
    }

    std::array<std::uint32_t, WARP_SIZE> integerResults{};
    std::array<bool, WARP_SIZE> predicateResults{};
    std::uint32_t participatingMask = warp_.active_mask_;
    if (instruction.predicate)
    {
        for (std::size_t laneIndex = 0; laneIndex < WARP_SIZE; ++laneIndex)
        {
            const std::uint32_t laneBit = std::uint32_t{1} << laneIndex;
            if ((participatingMask & laneBit) == 0) continue;
            const bool value = warp_.lanes[laneIndex].predicate_registers[instructionPredicate];
            if (value == instruction.predicate->negated) participatingMask &= ~laneBit;
        }
    }

    if (pc_ + 1 == program_.instructions.size())
    {
        const std::uint32_t remainingMask = isExit
            ? warp_.active_mask_ & ~participatingMask : warp_.active_mask_;
        if (remainingMask != 0)
            throw std::runtime_error("program ends while lanes remain active");
    }

    if (isExit)
    {
        warp_.active_mask_ &= ~participatingMask;
        advanceProgramCounterUnchecked();
        return;
    }

    for (std::size_t laneIndex = 0; laneIndex < WARP_SIZE; ++laneIndex)
    {
        const std::uint32_t laneBit = std::uint32_t{1} << laneIndex;
        if ((participatingMask & laneBit) == 0) continue;
        const LaneState& lane = warp_.lanes[laneIndex];
        if (hasDestinationPredicate)
        {
            const std::uint32_t leftBits = lane.general_purpose_registers[source1];
            const std::uint32_t rightBits = lane.general_purpose_registers[source2];
            if (isFloatComparison)
            {
                const float left = std::bit_cast<float>(leftBits);
                const float right = std::bit_cast<float>(rightBits);
                switch (opcode)
                {
                case Opcode::SETP_FEQ: predicateResults[laneIndex] = left == right; break;
                case Opcode::SETP_FNE: predicateResults[laneIndex] = left != right; break;
                case Opcode::SETP_FLT: predicateResults[laneIndex] = left < right; break;
                case Opcode::SETP_FLE: predicateResults[laneIndex] = left <= right; break;
                case Opcode::SETP_FGT: predicateResults[laneIndex] = left > right; break;
                case Opcode::SETP_FGE: predicateResults[laneIndex] = left >= right; break;
                default: throw std::runtime_error("unsupported floating comparison");
                }
            }
            else
            {
                const std::int32_t leftSigned = std::bit_cast<std::int32_t>(leftBits);
                const std::int32_t rightSigned = std::bit_cast<std::int32_t>(rightBits);
                switch (opcode)
                {
                case Opcode::SETP_EQ: predicateResults[laneIndex] = leftBits == rightBits; break;
                case Opcode::SETP_NE: predicateResults[laneIndex] = leftBits != rightBits; break;
                case Opcode::SETP_LT: predicateResults[laneIndex] = leftSigned < rightSigned; break;
                case Opcode::SETP_LE: predicateResults[laneIndex] = leftSigned <= rightSigned; break;
                case Opcode::SETP_GT: predicateResults[laneIndex] = leftSigned > rightSigned; break;
                case Opcode::SETP_GE: predicateResults[laneIndex] = leftSigned >= rightSigned; break;
                default: throw std::runtime_error("unsupported integer comparison");
                }
            }
            continue;
        }

        const std::uint32_t left = lane.general_purpose_registers[source1];
        const std::uint32_t right = source2IsRegister
            ? lane.general_purpose_registers[source2]
            : std::get<std::uint32_t>(instruction.operands[2]);
        switch (opcode)
        {
        case Opcode::ADD: integerResults[laneIndex] = left + right; break;
        case Opcode::SUB: integerResults[laneIndex] = left - right; break;
        case Opcode::MUL:
            integerResults[laneIndex] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(left) * right);
            break;
        case Opcode::AND: integerResults[laneIndex] = left & right; break;
        case Opcode::OR: integerResults[laneIndex] = left | right; break;
        case Opcode::SHL: integerResults[laneIndex] = left << (right & 31u); break;
        case Opcode::SHR: integerResults[laneIndex] = left >> (right & 31u); break;
        case Opcode::FADD:
        {
            const float a = std::bit_cast<float>(left);
            const float b = std::bit_cast<float>(right);
            integerResults[laneIndex] = std::bit_cast<std::uint32_t>(static_cast<float>(a + b));
            break;
        }
        case Opcode::FMUL:
        {
            const float a = std::bit_cast<float>(left);
            const float b = std::bit_cast<float>(right);
            integerResults[laneIndex] = std::bit_cast<std::uint32_t>(static_cast<float>(a * b));
            break;
        }
        case Opcode::FMA:
        {
            const float a = std::bit_cast<float>(left);
            const float b = std::bit_cast<float>(right);
            const float c = std::bit_cast<float>(lane.general_purpose_registers[source3]);
            integerResults[laneIndex] = std::bit_cast<std::uint32_t>(std::fma(a, b, c));
            break;
        }
        default: throw std::runtime_error("unsupported instruction in execution state");
        }
    }

    if (hasDestinationPredicate)
    {
        for (std::size_t laneIndex = 0; laneIndex < WARP_SIZE; ++laneIndex)
            if ((participatingMask & (std::uint32_t{1} << laneIndex)) != 0)
                warp_.lanes[laneIndex].predicate_registers[predicateDestination] = predicateResults[laneIndex];
    }
    else
    {
        for (std::size_t laneIndex = 0; laneIndex < WARP_SIZE; ++laneIndex)
            if ((participatingMask & (std::uint32_t{1} << laneIndex)) != 0)
                warp_.lanes[laneIndex].general_purpose_registers[destination] = integerResults[laneIndex];
    }
    advanceProgramCounterUnchecked();
}

} // namespace simtlab
