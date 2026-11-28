#include <simtlab/isa/execution_state.hpp>
#include <simtlab/isa/lexer.hpp>
#include <simtlab/isa/parser.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <variant>

int main()
{
    using namespace simtlab;
    constexpr std::string_view source =
        "ADD r3, r1, 32\n"
        "SETP.LT p0, r1, r2\n"
        "@p0 SUB r3, r3, 1\n"
        "EXIT\n";

    const auto tokens = lex(source);
    if (tokens.empty() || tokens.back().kind != TokenKind::EndOfFile) return 1;

    // parse(source) lexes the source internally; this is an independent lexer check.
    Program program = parse(source);
    if (program.instructions.size() != 4 || !program.labels.empty()
        || program.instructions[0].opcode != Opcode::ADD
        || program.instructions[1].opcode != Opcode::SETP_LT
        || program.instructions[2].opcode != Opcode::SUB
        || program.instructions[3].opcode != Opcode::EXIT) return 1;

    const auto& add = program.instructions[0];
    const auto& setp = program.instructions[1];
    const auto& sub = program.instructions[2];
    const auto& exit = program.instructions[3];
    const auto hasOperand = [](const Instruction& instruction, std::size_t index, auto expected)
    {
        if (index >= instruction.operands.size()) return false;
        const auto* actual = std::get_if<decltype(expected)>(&instruction.operands[index]);
        return actual != nullptr && *actual == expected;
    };
    if (add.operands.size() != 3
        || !hasOperand(add, 0, GprIndex::R3) || !hasOperand(add, 1, GprIndex::R1)
        || !hasOperand(add, 2, std::uint32_t{32}) || add.predicate
        || setp.operands.size() != 3
        || !hasOperand(setp, 0, PredicateIndex::P0) || !hasOperand(setp, 1, GprIndex::R1)
        || !hasOperand(setp, 2, GprIndex::R2) || setp.predicate
        || sub.operands.size() != 3
        || !hasOperand(sub, 0, GprIndex::R3) || !hasOperand(sub, 1, GprIndex::R3)
        || !hasOperand(sub, 2, std::uint32_t{1})
        || !sub.predicate
        || program.instructions[2].predicate->index != PredicateIndex::P0
        || sub.predicate->negated || !exit.operands.empty() || exit.predicate) return 1;

    ExecutionState state(std::move(program));
    for (std::uint32_t lane = 0; lane < WARP_SIZE; ++lane)
    {
        auto& values = state.lanes()[lane];
        for (std::uint32_t reg = 0; reg < GPR_COUNT; ++reg)
            values.general_purpose_registers[reg] = 0x5a000000u | (lane << 8u) | reg;
        values.general_purpose_registers[1] = 0xfffffff0u + lane; // signed -16 through 15
        values.general_purpose_registers[2] = 0xfffffff8u;        // signed -8 threshold
        values.general_purpose_registers[3] = 0xa5a5a5a5u;
        values.predicate_registers = {false, lane % 2 != 0, true, false};
    }

    constexpr std::size_t instructionCount = 4;
    std::size_t executed = 0;
    while (!state.terminated() && executed < instructionCount + 1)
    {
        const auto previousPc = state.pc();
        state.step();
        if (state.pc() != previousPc + 1) return 1;
        ++executed;
    }

    if (executed != 4 || state.pc() != 4 || !state.terminated() || state.activeMask() != 0)
        return 1;
    for (std::uint32_t lane = 0; lane < WARP_SIZE; ++lane)
    {
        const bool predicate = lane < 8;
        const std::uint32_t input = 0xfffffff0u + lane;
        const std::uint32_t afterAdd = input + 32u;
        const std::uint32_t expectedOutput = predicate ? afterAdd - 1u : afterAdd;
        const auto& values = state.lanes()[lane];
        if (values.predicate_registers[0] != predicate
            || values.predicate_registers[1] != (lane % 2 != 0)
            || !values.predicate_registers[2] || values.predicate_registers[3]
            || values.general_purpose_registers[1] != input
            || values.general_purpose_registers[2] != 0xfffffff8u
            || values.general_purpose_registers[3] != expectedOutput) return 1;
        for (std::uint32_t reg = 0; reg < GPR_COUNT; ++reg)
            if (reg != 1 && reg != 2 && reg != 3
                && values.general_purpose_registers[reg] != (0x5a000000u | (lane << 8u) | reg))
                return 1;
    }
    return 0;
}
