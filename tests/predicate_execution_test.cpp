#include <simtlab/isa/execution_state.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

using namespace simtlab;

template <typename T>
concept HasPublicActiveMask = requires(T& value) { value.active_mask; };
template <typename T>
concept HasWarpAccessor = requires(T& value) { value.warp(); };
static_assert(!HasPublicActiveMask<Warp>);
static_assert(!HasWarpAccessor<ExecutionState>);
static_assert(std::is_default_constructible_v<Warp>);
static_assert(std::is_move_constructible_v<Warp>);
static_assert(std::is_move_assignable_v<Warp>);
static_assert(std::is_assignable_v<decltype(std::declval<ExecutionState&>().lanes()[0]
    .general_purpose_registers[0]), std::uint32_t>);
static_assert(std::is_assignable_v<decltype(std::declval<ExecutionState&>().lanes()[0]
    .predicate_registers[0]), bool>);
static_assert(std::is_copy_constructible_v<ExecutionState>);
static_assert(std::is_move_constructible_v<ExecutionState>);
static_assert(!std::is_copy_assignable_v<ExecutionState>);
static_assert(!std::is_move_assignable_v<ExecutionState>);
static_assert(!std::is_swappable_v<ExecutionState>);

bool integerComparison(Opcode opcode)
{
    constexpr std::array<std::int32_t, 8> values = {0, 1, -1, -2147483647 - 1,
        2147483647, -2, 2, 0x40000000};
    constexpr std::array<std::int32_t, 8> otherValues = {0, -1, 1, 2147483647,
        -2147483647 - 1, -2, 3, -0x40000000};
    Program program;
    program.instructions.push_back({opcode, {PredicateIndex::P3, GprIndex::R1, GprIndex::R2}, {}});
    program.instructions.push_back({Opcode::EXIT, {}, {}});
    ExecutionState state(std::move(program));
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
    {
        auto& registers = state.lanes()[lane].general_purpose_registers;
        const std::int32_t left = values[lane % values.size()];
        const std::int32_t right = otherValues[(lane * 3) % otherValues.size()];
        registers[1] = std::bit_cast<std::uint32_t>(left);
        registers[2] = std::bit_cast<std::uint32_t>(right);
        state.lanes()[lane].predicate_registers[0] = lane % 2 != 0;
        state.lanes()[lane].predicate_registers[1] = lane % 3 == 0;
        state.lanes()[lane].predicate_registers[3] = true;
    }
    state.step();
    if (state.pc() != 1 || state.terminated()) return false;
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
    {
        const auto& registers = state.lanes()[lane].general_purpose_registers;
        const auto leftBits = registers[1];
        const auto rightBits = registers[2];
        const auto left = std::bit_cast<std::int32_t>(leftBits);
        const auto right = std::bit_cast<std::int32_t>(rightBits);
        bool expected = false;
        switch (opcode)
        {
        case Opcode::SETP_EQ: expected = leftBits == rightBits; break;
        case Opcode::SETP_NE: expected = leftBits != rightBits; break;
        case Opcode::SETP_LT: expected = left < right; break;
        case Opcode::SETP_LE: expected = left <= right; break;
        case Opcode::SETP_GT: expected = left > right; break;
        case Opcode::SETP_GE: expected = left >= right; break;
        default: return false;
        }
        if (state.lanes()[lane].predicate_registers[3] != expected
            || state.lanes()[lane].predicate_registers[0] != (lane % 2 != 0)
            || state.lanes()[lane].predicate_registers[1] != (lane % 3 == 0)
            || registers[1] != leftBits || registers[2] != rightBits) return false;
    }
    return true;
}

bool floatComparison(Opcode opcode)
{
    constexpr std::array<float, 8> values = {0.0f, -0.0f, -2.0f, 3.0f, 5.0f, -7.0f, 1.0f,
        std::bit_cast<float>(std::uint32_t{0x7fc00000u})};
    Program program;
    program.instructions.push_back({opcode, {PredicateIndex::P0, GprIndex::R1, GprIndex::R2}, {}});
    program.instructions.push_back({Opcode::EXIT, {}, {}});
    ExecutionState state(std::move(program));
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
    {
        auto& registers = state.lanes()[lane].general_purpose_registers;
        registers[1] = std::bit_cast<std::uint32_t>(values[lane % values.size()]);
        registers[2] = std::bit_cast<std::uint32_t>(values[(lane * 3 + 1) % values.size()]);
        state.lanes()[lane].predicate_registers[1] = lane % 2 != 0;
        state.lanes()[lane].predicate_registers[2] = lane % 3 == 0;
        state.lanes()[lane].predicate_registers[3] = true;
    }
    state.step();
    if (state.pc() != 1 || state.terminated()) return false;
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
    {
        const auto& registers = state.lanes()[lane].general_purpose_registers;
        const float left = std::bit_cast<float>(registers[1]);
        const float right = std::bit_cast<float>(registers[2]);
        bool expected = false;
        switch (opcode)
        {
        case Opcode::SETP_FEQ: expected = left == right; break;
        case Opcode::SETP_FNE: expected = left != right; break;
        case Opcode::SETP_FLT: expected = left < right; break;
        case Opcode::SETP_FLE: expected = left <= right; break;
        case Opcode::SETP_FGT: expected = left > right; break;
        case Opcode::SETP_FGE: expected = left >= right; break;
        default: return false;
        }
        if (state.lanes()[lane].predicate_registers[0] != expected
            || state.lanes()[lane].predicate_registers[1] != (lane % 2 != 0)
            || state.lanes()[lane].predicate_registers[2] != (lane % 3 == 0)
            || !state.lanes()[lane].predicate_registers[3]) return false;
    }
    return true;
}

using RegisterFiles = std::array<std::array<std::uint32_t, GPR_COUNT>, WARP_SIZE>;
using PredicateFiles = std::array<std::array<bool, PREDICATE_COUNT>, WARP_SIZE>;

void snapshot(const ExecutionState& state, RegisterFiles& registers, PredicateFiles& predicates)
{
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
    {
        registers[lane] = state.lanes()[lane].general_purpose_registers;
        predicates[lane] = state.lanes()[lane].predicate_registers;
    }
}

bool unchanged(const ExecutionState& state, const RegisterFiles& registers,
               const PredicateFiles& predicates)
{
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
        if (state.lanes()[lane].general_purpose_registers != registers[lane]
            || state.lanes()[lane].predicate_registers != predicates[lane]) return false;
    return true;
}

bool rejectsUnsupported(Instruction instruction)
{
    Program program;
    instruction.predicate = Predicate{PredicateIndex::P0, false}; // False predicate must not hide unsupported opcodes.
    program.instructions.push_back(std::move(instruction));
    program.instructions.push_back({Opcode::EXIT, {}, {}});
    ExecutionState state(std::move(program));
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
    {
        auto& values = state.lanes()[lane];
        values.predicate_registers = {false, lane % 2 == 0, lane % 3 == 0, true};
        for (std::size_t reg = 0; reg < GPR_COUNT; ++reg)
            values.general_purpose_registers[reg] = static_cast<std::uint32_t>(lane * 101 + reg);
    }
    RegisterFiles registers{};
    PredicateFiles predicates{};
    snapshot(state, registers, predicates);
    try { state.step(); return false; }
    catch (const std::runtime_error& error)
    {
        return std::string(error.what()).find("unsupported instruction") != std::string::npos
            && state.pc() == 0 && !state.terminated() && state.activeMask() == 0xffffffffu
            && unchanged(state, registers, predicates);
    }
}

int main()
{
    for (const auto opcode : {Opcode::SETP_EQ, Opcode::SETP_NE, Opcode::SETP_LT,
                              Opcode::SETP_LE, Opcode::SETP_GT, Opcode::SETP_GE})
        if (!integerComparison(opcode)) return 1;
    for (const auto opcode : {Opcode::SETP_FEQ, Opcode::SETP_FNE, Opcode::SETP_FLT,
                              Opcode::SETP_FLE, Opcode::SETP_FGT, Opcode::SETP_FGE})
        if (!floatComparison(opcode)) return 1;

    ExecutionState predicated(parse("@p0 ADD r3, r1, r2\n@!p1 SUB r4, r1, r2\nEXIT"));
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
    {
        auto& state = predicated.lanes()[lane];
        state.general_purpose_registers[1] = static_cast<std::uint32_t>(lane + 10);
        state.general_purpose_registers[2] = static_cast<std::uint32_t>(lane + 3);
        state.general_purpose_registers[3] = 99;
        state.general_purpose_registers[4] = 88;
        state.predicate_registers[0] = lane % 2 == 0;
        state.predicate_registers[1] = lane % 3 == 0;
    }
    RegisterFiles arithmeticRegisters{};
    PredicateFiles arithmeticPredicates{};
    snapshot(predicated, arithmeticRegisters, arithmeticPredicates);
    predicated.step();
    if (predicated.pc() != 1 || predicated.terminated()) return 1;
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
    {
        const auto& values = predicated.lanes()[lane];
        if (lane % 2 == 0)
        {
            if (values.general_purpose_registers[3]
                != arithmeticRegisters[lane][1] + arithmeticRegisters[lane][2]) return 1;
            for (std::size_t reg = 0; reg < GPR_COUNT; ++reg)
                if (reg != 3 && values.general_purpose_registers[reg] != arithmeticRegisters[lane][reg]) return 1;
            if (values.predicate_registers != arithmeticPredicates[lane]) return 1;
        }
        else if (values.general_purpose_registers != arithmeticRegisters[lane]
                 || values.predicate_registers != arithmeticPredicates[lane]) return 1;
    }
    predicated.step();
    if (predicated.pc() != 2 || predicated.terminated()) return 1;
    predicated.step();
    if (predicated.pc() != 3 || !predicated.terminated()) return 1;
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
    {
        const auto& registers = predicated.lanes()[lane].general_purpose_registers;
        if (registers[3] != (lane % 2 == 0 ? 2 * lane + 13 : 99)
            || registers[4] != (lane % 3 != 0 ? 7 : 88)
            || registers[1] != lane + 10 || registers[2] != lane + 3) return 1;
    }

    ExecutionState allSelected(parse("@p0 ADD r5, r1, r2\nEXIT"));
    for (auto& lane : allSelected.lanes())
    {
        lane.predicate_registers[0] = true;
        lane.general_purpose_registers[1] = 12;
        lane.general_purpose_registers[2] = 30;
    }
    allSelected.step();
    if (allSelected.pc() != 1 || allSelected.activeMask() != 0xffffffffu) return 1;
    for (const auto& lane : allSelected.lanes())
        if (lane.general_purpose_registers[5] != 42) return 1;

    ExecutionState predicatedLogical(parse("@p0 AND r5, r1, r2\nEXIT"));
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
    {
        auto& values = predicatedLogical.lanes()[lane];
        values.predicate_registers = {lane % 2 == 0, lane % 3 == 0, true, false};
        for (std::size_t reg = 0; reg < GPR_COUNT; ++reg)
            values.general_purpose_registers[reg] = static_cast<std::uint32_t>(lane * 100 + reg);
    }
    RegisterFiles logicalRegisters{};
    PredicateFiles logicalPredicates{};
    snapshot(predicatedLogical, logicalRegisters, logicalPredicates);
    predicatedLogical.step();
    if (predicatedLogical.pc() != 1 || predicatedLogical.terminated()) return 1;
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
    {
        const auto& values = predicatedLogical.lanes()[lane];
        if (lane % 2 == 0)
        {
            if (values.general_purpose_registers[5]
                != (logicalRegisters[lane][1] & logicalRegisters[lane][2])) return 1;
            for (std::size_t reg = 0; reg < GPR_COUNT; ++reg)
                if (reg != 5 && values.general_purpose_registers[reg] != logicalRegisters[lane][reg]) return 1;
            if (values.predicate_registers != logicalPredicates[lane]) return 1;
        }
        else if (values.general_purpose_registers != logicalRegisters[lane]
                 || values.predicate_registers != logicalPredicates[lane]) return 1;
    }

    ExecutionState partialFinalExit(parse("@p0 EXIT"));
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
    {
        auto& values = partialFinalExit.lanes()[lane];
        values.predicate_registers = {lane % 2 == 0, true, false, true};
        values.general_purpose_registers[7] = static_cast<std::uint32_t>(lane + 70);
    }
    RegisterFiles exitRegisters{};
    PredicateFiles exitPredicates{};
    snapshot(partialFinalExit, exitRegisters, exitPredicates);
    try { partialFinalExit.step(); return 1; }
    catch (const std::runtime_error& error)
    {
        if (std::string(error.what()).find("at 1:1") == std::string::npos) return 1;
    }
    if (partialFinalExit.pc() != 0 || partialFinalExit.terminated()
        || partialFinalExit.activeMask() != 0xffffffffu
        || !unchanged(partialFinalExit, exitRegisters, exitPredicates)) return 1;

    ExecutionState allExit(parse("@!p0 EXIT"));
    for (auto& lane : allExit.lanes()) lane.predicate_registers[0] = false;
    allExit.step();
    if (allExit.pc() != 1 || !allExit.terminated() || allExit.activeMask() != 0) return 1;

    ExecutionState finalNoParticipants(parse("@p0 ADD r5, r1, r2"));
    for (auto& lane : finalNoParticipants.lanes())
    {
        lane.predicate_registers = {false, true, false, true};
        lane.general_purpose_registers[5] = 123;
    }
    RegisterFiles noParticipantRegisters{};
    PredicateFiles noParticipantPredicates{};
    snapshot(finalNoParticipants, noParticipantRegisters, noParticipantPredicates);
    try { finalNoParticipants.step(); return 1; }
    catch (const std::runtime_error& error)
    {
        if (std::string(error.what()).find("program ends while lanes remain active at 1:1")
            == std::string::npos) return 1;
    }
    if (finalNoParticipants.pc() != 0 || finalNoParticipants.terminated()
        || finalNoParticipants.activeMask() != 0xffffffffu
        || !unchanged(finalNoParticipants, noParticipantRegisters, noParticipantPredicates)) return 1;

    ExecutionState predicatedCompare(parse("@p3 SETP.FNE p0, r1, r2\nEXIT"));
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
    {
        auto& state = predicatedCompare.lanes()[lane];
        state.predicate_registers[3] = lane % 2 == 0;
        state.predicate_registers[0] = true;
        state.general_purpose_registers[1] = 1;
        state.general_purpose_registers[2] = lane == 0 ? 1 : 2;
    }
    predicatedCompare.step();
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
        if (predicatedCompare.lanes()[lane].predicate_registers[0]
            != (lane % 2 == 0 ? lane != 0 : true)) return 1;

    ExecutionState inactive(parse("@p0 ADD r3, r1, r2\nEXIT"));
    for (auto& lane : inactive.lanes())
    {
        lane.predicate_registers[0] = false;
        lane.general_purpose_registers[3] = 17;
    }
    inactive.step();
    if (inactive.pc() != 1 || inactive.terminated() || inactive.activeMask() != 0xffffffffu) return 1;
    for (const auto& lane : inactive.lanes())
        if (lane.general_purpose_registers[3] != 17) return 1;

    ExecutionState retire(parse("@p0 EXIT\nADD r3, r1, r2\nEXIT"));
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
    {
        auto& state = retire.lanes()[lane];
        state.predicate_registers[0] = lane % 2 == 0;
        state.general_purpose_registers[1] = 4;
        state.general_purpose_registers[2] = 5;
        state.general_purpose_registers[3] = 0;
    }
    retire.step();
    if (retire.pc() != 1 || retire.terminated() || retire.activeMask() != 0xaaaaaaaau) return 1;
    ExecutionState copiedPartial(retire);
    if (copiedPartial.pc() != retire.pc() || copiedPartial.program().instructions.size() != 3
        || copiedPartial.program().instructions[1].opcode != Opcode::ADD
        || copiedPartial.activeMask() != retire.activeMask() || copiedPartial.terminated()) return 1;
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
        if (copiedPartial.lanes()[lane].general_purpose_registers
                != retire.lanes()[lane].general_purpose_registers
            || copiedPartial.lanes()[lane].predicate_registers
                != retire.lanes()[lane].predicate_registers) return 1;
    ExecutionState movedPartial(std::move(copiedPartial));
    if (movedPartial.activeMask() != 0xaaaaaaaau || movedPartial.terminated()
        || movedPartial.pc() != retire.pc() || movedPartial.program().instructions.size() != 3
        || movedPartial.lanes()[1].general_purpose_registers[3] != 0
        || movedPartial.lanes()[0].general_purpose_registers[3] != 0
        || copiedPartial.activeMask() != 0 || !copiedPartial.terminated()
        || copiedPartial.pc() != 0 || !copiedPartial.program().instructions.empty()) return 1;
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
    {
        if (movedPartial.lanes()[lane].general_purpose_registers
                != retire.lanes()[lane].general_purpose_registers
            || movedPartial.lanes()[lane].predicate_registers
                != retire.lanes()[lane].predicate_registers) return 1;
        for (const auto value : copiedPartial.lanes()[lane].general_purpose_registers)
            if (value != 0) return 1;
        for (const auto value : copiedPartial.lanes()[lane].predicate_registers)
            if (value) return 1;
    }
    retire.step();
    for (std::size_t lane = 0; lane < WARP_SIZE; ++lane)
        if (retire.lanes()[lane].general_purpose_registers[3] != (lane % 2 == 0 ? 0 : 9)) return 1;
    retire.step();
    if (retire.pc() != 3 || !retire.terminated() || retire.activeMask() != 0) return 1;
    try { retire.step(); return 1; } catch (const std::out_of_range&) {}
    if (retire.pc() != 3) return 1;

    ExecutionState noRetire(parse("@p0 EXIT\nEXIT"));
    for (auto& lane : noRetire.lanes()) lane.predicate_registers[0] = false;
    noRetire.step();
    if (noRetire.pc() != 1 || noRetire.terminated() || noRetire.activeMask() != 0xffffffffu) return 1;
    noRetire.step();
    if (!noRetire.terminated() || noRetire.activeMask() != 0) return 1;

    ExecutionState allRetired(parse("EXIT\nADD r3, r1, r2"));
    allRetired.lanes()[0].general_purpose_registers[6] = 0x1234u;
    allRetired.lanes()[0].predicate_registers[2] = true;
    allRetired.step();
    if (allRetired.pc() != 1 || !allRetired.terminated() || allRetired.activeMask() != 0) return 1;
    try { allRetired.currentInstruction(); return 1; } catch (const std::out_of_range&) {}
    ExecutionState copiedRetired(allRetired);
    if (!copiedRetired.terminated() || copiedRetired.pc() != 1
        || copiedRetired.program().instructions.size() != 2
        || copiedRetired.lanes()[0].general_purpose_registers[6] != 0x1234u
        || !copiedRetired.lanes()[0].predicate_registers[2]) return 1;
    ExecutionState movedRetired(std::move(copiedRetired));
    if (!movedRetired.terminated() || movedRetired.activeMask() != 0
        || movedRetired.pc() != 1 || movedRetired.program().instructions.size() != 2
        || movedRetired.lanes()[0].general_purpose_registers[6] != 0x1234u
        || !movedRetired.lanes()[0].predicate_registers[2]
        || !copiedRetired.terminated() || copiedRetired.activeMask() != 0
        || copiedRetired.pc() != 0 || !copiedRetired.program().instructions.empty()) return 1;
    for (const auto& lane : copiedRetired.lanes())
    {
        for (const auto value : lane.general_purpose_registers) if (value != 0) return 1;
        for (const auto value : lane.predicate_registers) if (value) return 1;
    }

    ExecutionState fallsOff(parse("ADD r0, r1, r2"));
    fallsOff.lanes()[0].general_purpose_registers[0] = 19;
    fallsOff.lanes()[0].general_purpose_registers[1] = 3;
    fallsOff.lanes()[0].general_purpose_registers[2] = 4;
    try { fallsOff.step(); return 1; } catch (const std::runtime_error&) {}
    if (fallsOff.pc() != 0 || fallsOff.terminated()
        || fallsOff.lanes()[0].general_purpose_registers[0] != 19) return 1;

    Program invalidPredicate;
    invalidPredicate.instructions.push_back({Opcode::ADD, {GprIndex::R0, GprIndex::R1, GprIndex::R2},
                                              Predicate{static_cast<PredicateIndex>(9), false}, SourceLocation{9, 2}});
    ExecutionState invalid(std::move(invalidPredicate));
    invalid.lanes()[0].general_purpose_registers[0] = 42;
    try { invalid.step(); return 1; }
    catch (const std::runtime_error& error)
    {
        if (std::string(error.what()).find("invalid instruction predicate index at 9:2")
            == std::string::npos) return 1;
    }
    if (invalid.pc() != 0 || invalid.lanes()[0].general_purpose_registers[0] != 42) return 1;

    Program invalidRegister;
    invalidRegister.instructions.push_back({Opcode::ADD,
        {static_cast<GprIndex>(40), GprIndex::R1, GprIndex::R2}, {}, SourceLocation{4, 6}});
    ExecutionState badRegister(std::move(invalidRegister));
    try { badRegister.step(); return 1; }
    catch (const std::runtime_error& error)
    {
        if (std::string(error.what()).find("expected valid GPR operand at 4:6")
            == std::string::npos) return 1;
    }
    if (badRegister.pc() != 0 || badRegister.terminated()) return 1;

    Program unsupported;
    unsupported.instructions.push_back({Opcode::BRA, {LabelOperand{"target"}, LabelOperand{"target"}}, {}});
    ExecutionState rejected(std::move(unsupported));
    try { rejected.step(); return 1; } catch (const std::runtime_error&) {}
    if (rejected.pc() != 0) return 1;

    const std::array<Instruction, 9> unsupportedInstructions = {{
        {Opcode::BRA, {LabelOperand{"target"}, LabelOperand{"reconv"}}, {}},
        {Opcode::JMP, {LabelOperand{"target"}}, {}},
        {Opcode::LDG, {GprIndex::R0, MemoryOperand{GprIndex::R1, std::monostate{}}, MemoryWidth::WORD}, {}},
        {Opcode::STG, {MemoryOperand{GprIndex::R1, std::monostate{}}, GprIndex::R0, MemoryWidth::WORD}, {}},
        {Opcode::LDS, {GprIndex::R0, MemoryOperand{GprIndex::R1, std::monostate{}}, MemoryWidth::WORD}, {}},
        {Opcode::STS, {MemoryOperand{GprIndex::R1, std::monostate{}}, GprIndex::R0, MemoryWidth::WORD}, {}},
        {Opcode::BAR_SYNC, {std::uint32_t{0}}, {}},
        {Opcode::SREG, {GprIndex::R0, SpecialRegister::LANEID}, {}},
        {Opcode::MMA_M16N16K16, {GprIndex::R0, GprIndex::R8, GprIndex::R16}, {}}
    }};
    for (auto instruction : unsupportedInstructions)
        if (!rejectsUnsupported(std::move(instruction))) return 1;

    const auto located = parse("\n  SREG r0, LANEID\nEXIT");
    if (!located.instructions[0].source_location
        || located.instructions[0].source_location->line != 2
        || located.instructions[0].source_location->column != 3
        || located.instructions[1].source_location->line != 3) return 1;
    ExecutionState locatedError(located);
    try { locatedError.step(); return 1; }
    catch (const std::runtime_error& error)
    {
        if (std::string(error.what()).find("unsupported instruction in execution state at 2:3")
            == std::string::npos) return 1;
    }

    Program programmatic;
    programmatic.instructions.push_back({Opcode::ADD, {GprIndex::R0}, {}, SourceLocation{7, 4}});
    ExecutionState invalidShape(std::move(programmatic));
    try { invalidShape.step(); return 1; }
    catch (const std::runtime_error& error)
    {
        if (std::string(error.what()).find("invalid operand count for instruction at 7:4")
            == std::string::npos) return 1;
    }
    Program noLocation;
    noLocation.instructions.push_back({Opcode::SREG, {GprIndex::R0, SpecialRegister::LANEID}, {}});
    ExecutionState noLocationError(std::move(noLocation));
    try { noLocationError.step(); return 1; }
    catch (const std::runtime_error& error)
    {
        if (std::string(error.what()).find("source location unavailable") == std::string::npos) return 1;
    }
    return 0;
}
