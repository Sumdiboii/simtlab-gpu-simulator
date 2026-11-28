#include <simtlab/isa/execution_state.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

using namespace simtlab;

template<class Operation>
bool integerRegisters(std::string_view source, Opcode opcode, Operation operation)
{
    ExecutionState state(parse(std::string(source) + "\nEXIT"));
    if (state.currentInstruction().opcode != opcode || state.pc() != 0) return false;
    for (std::size_t i = 0; i < WARP_SIZE; ++i)
    {
        auto& registers = state.lanes()[i].general_purpose_registers;
        registers[1] = 0xfffffff0u + static_cast<std::uint32_t>(i);
        registers[2] = static_cast<std::uint32_t>(i + 1);
        registers[3] = 0x12345678u;
    }
    state.step();
    if (state.pc() != 1 || state.terminated()) return false;
    for (std::size_t i = 0; i < WARP_SIZE; ++i)
    {
        const auto& registers = state.lanes()[i].general_purpose_registers;
        if (registers[3] != operation(0xfffffff0u + static_cast<std::uint32_t>(i),
                                      static_cast<std::uint32_t>(i + 1))
            || registers[1] != 0xfffffff0u + static_cast<std::uint32_t>(i)
            || registers[2] != i + 1) return false;
    }
    return true;
}

template<class Operation>
bool integerImmediate(std::string_view source, Opcode opcode, std::uint32_t immediate, Operation operation)
{
    ExecutionState state(parse(std::string(source) + "\nEXIT"));
    for (std::size_t i = 0; i < WARP_SIZE; ++i)
        state.lanes()[i].general_purpose_registers[1] = 0xfffffff0u + static_cast<std::uint32_t>(i);
    if (state.currentInstruction().opcode != opcode) return false;
    state.step();
    if (state.pc() != 1 || state.terminated()) return false;
    for (std::size_t i = 0; i < WARP_SIZE; ++i)
    {
        const auto& registers = state.lanes()[i].general_purpose_registers;
        if (registers[3] != operation(0xfffffff0u + static_cast<std::uint32_t>(i), immediate)
            || registers[1] != 0xfffffff0u + static_cast<std::uint32_t>(i)) return false;
    }
    return true;
}

bool floatingInstruction(std::string_view source, Opcode opcode, int operation)
{
    ExecutionState state(parse(std::string(source) + "\nEXIT"));
    std::array<std::array<std::uint32_t, 3>, WARP_SIZE> original{};
    for (std::size_t i = 0; i < WARP_SIZE; ++i)
    {
        auto& registers = state.lanes()[i].general_purpose_registers;
        const float a = 1.25f + static_cast<float>(i);
        const float b = 0.5f + static_cast<float>(i) * 0.125f;
        registers[1] = std::bit_cast<std::uint32_t>(operation == 2 && i == 0
            ? std::bit_cast<float>(std::uint32_t{0x3f800001u}) : a);
        registers[2] = std::bit_cast<std::uint32_t>(operation == 2 && i == 0
            ? std::bit_cast<float>(std::uint32_t{0x3f7fffffu}) : b);
        registers[3] = std::bit_cast<std::uint32_t>(operation == 2 && i == 0 ? -1.0f : 0.25f);
        original[i] = {registers[1], registers[2], registers[3]};
    }
    if (state.currentInstruction().opcode != opcode) return false;
    std::uint32_t expected[WARP_SIZE]{};
    for (std::size_t i = 0; i < WARP_SIZE; ++i)
    {
        const auto& registers = state.lanes()[i].general_purpose_registers;
        const float a = std::bit_cast<float>(registers[1]);
        const float b = std::bit_cast<float>(registers[2]);
        const float c = std::bit_cast<float>(registers[3]);
        const float separateProduct = static_cast<float>(a * b);
        const float separateResult = static_cast<float>(separateProduct + c);
        const float result = operation == 0 ? static_cast<float>(a + b)
            : operation == 1 ? static_cast<float>(a * b) : std::fma(a, b, c);
        expected[i] = std::bit_cast<std::uint32_t>(result);
        if (operation == 2 && i == 0 && result == separateResult) return false;
    }
    const auto oldSource1 = state.lanes()[0].general_purpose_registers[1];
    state.step();
    if (state.pc() != 1 || state.terminated()) return false;
    for (std::size_t i = 0; i < WARP_SIZE; ++i)
    {
        const auto& registers = state.lanes()[i].general_purpose_registers;
        if (registers[4] != expected[i] || registers[1] != original[i][0]
            || registers[2] != original[i][1] || registers[3] != original[i][2])
            return false;
    }
    return state.lanes()[0].general_purpose_registers[1] == oldSource1;
}

int main()
{
    if (!integerRegisters("ADD r3, r1, r2", Opcode::ADD, [](auto a, auto b) { return a + b; })
        || !integerRegisters("SUB r3, r1, r2", Opcode::SUB, [](auto a, auto b) { return a - b; })
        || !integerRegisters("MUL r3, r1, r2", Opcode::MUL,
                            [](auto a, auto b) { return static_cast<std::uint32_t>(std::uint64_t{a} * b); })
        || !integerRegisters("AND r3, r1, r2", Opcode::AND, [](auto a, auto b) { return a & b; })
        || !integerRegisters("OR r3, r1, r2", Opcode::OR, [](auto a, auto b) { return a | b; })
        || !integerRegisters("SHL r3, r1, r2", Opcode::SHL, [](auto a, auto b) { return a << (b & 31u); })
        || !integerRegisters("SHR r3, r1, r2", Opcode::SHR, [](auto a, auto b) { return a >> (b & 31u); })) return 1;

    if (!integerImmediate("AND r3, r1, 0xF0F0", Opcode::AND, 0xf0f0u,
                          [](auto a, auto b) { return a & b; })
        || !integerImmediate("OR r3, r1, -1", Opcode::OR, 0xffffffffu,
                             [](auto a, auto b) { return a | b; })
        || !integerImmediate("SHL r3, r1, 32", Opcode::SHL, 32,
                             [](auto a, auto b) { return a << (b & 31u); })
        || !integerImmediate("SHR r3, r1, -0x1", Opcode::SHR, 0xffffffffu,
                             [](auto a, auto b) { return a >> (b & 31u); })
        || !integerImmediate("ADD r3, r1, -1", Opcode::ADD, 0xffffffffu,
                             [](auto a, auto b) { return a + b; })
        || !integerImmediate("ADD r3, r1, 4294967295", Opcode::ADD, 0xffffffffu,
                             [](auto a, auto b) { return a + b; })
        || !integerImmediate("SUB r3, r1, -2", Opcode::SUB, 0xfffffffeu,
                             [](auto a, auto b) { return a - b; })) return 1;

    for (const auto& [opcode, immediate, subtract] : {
             std::tuple{Opcode::ADD, std::uint32_t{0xffffffffu}, false},
             std::tuple{Opcode::SUB, std::uint32_t{0x80000000u}, true}})
    {
        Program program;
        program.instructions.push_back({opcode, {GprIndex::R3, GprIndex::R1, immediate}, {}});
        program.instructions.push_back({Opcode::EXIT, {}, {}});
        ExecutionState state(std::move(program));
        for (std::size_t i = 0; i < WARP_SIZE; ++i)
            state.lanes()[i].general_purpose_registers[1] = static_cast<std::uint32_t>(i + 2);
        state.step();
        if (state.pc() != 1 || state.terminated()) return 1;
        for (std::size_t i = 0; i < WARP_SIZE; ++i)
        {
            const std::uint32_t a = static_cast<std::uint32_t>(i + 2);
            const std::uint32_t expected = subtract ? a - immediate : a + immediate;
            if (state.lanes()[i].general_purpose_registers[3] != expected) return 1;
        }
    }

    if (!floatingInstruction("FADD r4, r1, r2", Opcode::FADD, 0)
        || !floatingInstruction("FMUL r4, r1, r2", Opcode::FMUL, 1)
        || !floatingInstruction("FMA r4, r1, r2, r3", Opcode::FMA, 2)) return 1;

    ExecutionState alias(parse("ADD r1, r1, r2\nEXIT"));
    for (std::size_t i = 0; i < WARP_SIZE; ++i)
    {
        alias.lanes()[i].general_purpose_registers[1] = 10u + static_cast<std::uint32_t>(i);
        alias.lanes()[i].general_purpose_registers[2] = 20u + static_cast<std::uint32_t>(i);
    }
    alias.step();
    for (std::size_t i = 0; i < WARP_SIZE; ++i)
        if (alias.lanes()[i].general_purpose_registers[1] != 30u + 2u * static_cast<std::uint32_t>(i)
            || alias.lanes()[i].general_purpose_registers[2] != 20u + static_cast<std::uint32_t>(i)) return 1;

    ExecutionState aliasSource2(parse("MUL r2, r1, r2\nEXIT"));
    for (std::size_t i = 0; i < WARP_SIZE; ++i)
    {
        aliasSource2.lanes()[i].general_purpose_registers[1] = 3u + static_cast<std::uint32_t>(i);
        aliasSource2.lanes()[i].general_purpose_registers[2] = 5u + static_cast<std::uint32_t>(i);
    }
    aliasSource2.step();
    for (std::size_t i = 0; i < WARP_SIZE; ++i)
        if (aliasSource2.lanes()[i].general_purpose_registers[2]
            != (3u + static_cast<std::uint32_t>(i)) * (5u + static_cast<std::uint32_t>(i))) return 1;

    ExecutionState unsupported(parse("LDG r0, [r1], 4"));
    unsupported.lanes()[0].general_purpose_registers[0] = 9;
    try { unsupported.step(); return 1; }
    catch (const std::runtime_error&) {}
    if (unsupported.pc() != 0 || unsupported.terminated()
        || unsupported.lanes()[0].general_purpose_registers[0] != 9) return 1;

    Program invalidRegisterProgram;
    invalidRegisterProgram.instructions.push_back(
        {Opcode::ADD, {static_cast<GprIndex>(255), GprIndex::R1, GprIndex::R2}, {}});
    ExecutionState invalidRegister(std::move(invalidRegisterProgram));
    try { invalidRegister.step(); return 1; }
    catch (const std::runtime_error&) {}
    if (invalidRegister.pc() != 0 || invalidRegister.terminated()) return 1;

    Program malformed;
    malformed.instructions.push_back({Opcode::ADD, {GprIndex::R0, GprIndex::R1}, {}});
    ExecutionState invalid(std::move(malformed));
    try { invalid.step(); return 1; }
    catch (const std::runtime_error&) {}
    if (invalid.pc() != 0 || invalid.terminated()) return 1;
    return 0;
}
