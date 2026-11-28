#include <simtlab/isa/execution_state.hpp>

#include <cstdint>
#include <stdexcept>
#include <utility>

int main()
{
    simtlab::ExecutionState state(simtlab::parse("EXIT\nJMP done\ndone:\nEXIT"));
    if (state.pc() != 0 || state.terminated() || state.program().instructions.size() != 3
        || state.program().labels.at("done") != 2 || state.currentInstruction().opcode != simtlab::Opcode::EXIT
        || state.lanes().size() != simtlab::WARP_SIZE) return 1;
    for (const auto& lane : state.lanes())
    {
        if (lane.general_purpose_registers.size() != simtlab::GPR_COUNT
            || lane.predicate_registers.size() != simtlab::PREDICATE_COUNT) return 1;
        for (const auto value : lane.general_purpose_registers)
            if (value != std::uint32_t{0}) return 1;
        for (const auto value : lane.predicate_registers)
            if (value) return 1;
    }
    state.advanceProgramCounter();
    if (state.pc() != 1 || state.terminated() || state.currentInstruction().opcode != simtlab::Opcode::JMP) return 1;
    state.advanceProgramCounter();
    if (state.pc() != 2 || state.terminated() || state.currentInstruction().opcode != simtlab::Opcode::EXIT) return 1;
    try { state.advanceProgramCounter(); return 1; }
    catch (const std::runtime_error&) {}
    if (state.pc() != 2 || state.terminated()
        || state.currentInstruction().opcode != simtlab::Opcode::EXIT) return 1;

    simtlab::ExecutionState empty(simtlab::parse(""));
    if (empty.pc() != 0 || !empty.terminated() || !empty.program().instructions.empty()) return 1;
    try { empty.currentInstruction(); return 1; }
    catch (const std::out_of_range&) {}

    const auto sourceProgram = simtlab::parse("EXIT\nEXIT");
    simtlab::ExecutionState copiedProgram(sourceProgram);
    if (sourceProgram.instructions.size() != 2 || copiedProgram.program().instructions.size() != 2) return 1;
    simtlab::ExecutionState copiedState(copiedProgram);
    if (copiedState.pc() != 0 || copiedState.terminated()
        || copiedState.activeMask() != 0xffffffffu
        || copiedState.program().instructions[0].opcode != simtlab::Opcode::EXIT) return 1;
    copiedState.advanceProgramCounter();
    if (copiedState.pc() != 1 || copiedProgram.pc() != 0) return 1;

    copiedState.lanes()[4].general_purpose_registers[8] = 91;
    copiedState.lanes()[4].predicate_registers[2] = true;
    simtlab::ExecutionState copiedFresh(copiedState);
    if (copiedFresh.pc() != 1 || copiedFresh.program().instructions.size() != 2
        || copiedFresh.activeMask() != 0xffffffffu
        || copiedFresh.lanes()[4].general_purpose_registers[8] != 91
        || !copiedFresh.lanes()[4].predicate_registers[2]) return 1;

    simtlab::ExecutionState freshMoveSource(simtlab::parse("EXIT\nEXIT"));
    freshMoveSource.lanes()[7].general_purpose_registers[9] = 123;
    freshMoveSource.lanes()[7].predicate_registers[1] = true;
    simtlab::ExecutionState freshMoved(std::move(freshMoveSource));
    if (freshMoved.pc() != 0 || freshMoved.program().instructions.size() != 2
        || freshMoved.activeMask() != 0xffffffffu
        || freshMoved.lanes()[7].general_purpose_registers[9] != 123
        || !freshMoved.lanes()[7].predicate_registers[1]
        || freshMoveSource.pc() != 0 || !freshMoveSource.terminated()
        || !freshMoveSource.program().instructions.empty()
        || freshMoveSource.activeMask() != 0) return 1;
    for (const auto& lane : freshMoveSource.lanes())
    {
        for (const auto value : lane.general_purpose_registers) if (value != 0) return 1;
        for (const auto value : lane.predicate_registers) if (value) return 1;
    }

    simtlab::ExecutionState moveSource(simtlab::parse("entry:\nEXIT\nEXIT"));
    moveSource.advanceProgramCounter();
    moveSource.lanes()[0].general_purpose_registers[0] = 7;
    moveSource.lanes()[0].predicate_registers[3] = true;
    simtlab::ExecutionState moved(std::move(moveSource));
    if (moved.pc() != 1 || moved.terminated() || moved.program().labels.at("entry") != 0
        || moved.lanes()[0].general_purpose_registers[0] != 7
        || !moved.lanes()[0].predicate_registers[3]
        || moveSource.pc() != 0 || !moveSource.terminated() || !moveSource.program().instructions.empty()
        || !moveSource.program().labels.empty()
        || moveSource.lanes()[0].general_purpose_registers[0] != 0
        || moveSource.lanes()[0].predicate_registers[3]
        || moveSource.activeMask() != 0) return 1;
    try { moveSource.currentInstruction(); return 1; }
    catch (const std::out_of_range&) {}
    try { moveSource.advanceProgramCounter(); return 1; }
    catch (const std::out_of_range&) {}

    return 0;
}
