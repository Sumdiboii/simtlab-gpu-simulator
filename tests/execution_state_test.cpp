#include <simtlab/isa/execution_state.hpp>

#include <cstdint>
#include <stdexcept>
#include <utility>

int main()
{
    simtlab::ExecutionState state(simtlab::parse("EXIT\nJMP done\ndone:\nEXIT"));
    if (state.pc() != 0 || state.terminated() || state.program().instructions.size() != 3
        || state.program().labels.at("done") != 2 || state.currentInstruction().opcode != simtlab::Opcode::EXIT
        || state.warp().lanes.size() != simtlab::WARP_SIZE) return 1;
    for (const auto& lane : state.warp().lanes)
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
    state.advanceProgramCounter();
    if (state.pc() != 3 || !state.terminated()) return 1;
    try { state.currentInstruction(); return 1; }
    catch (const std::out_of_range&) {}
    try { state.advanceProgramCounter(); return 1; }
    catch (const std::out_of_range&) {}

    simtlab::ExecutionState empty(simtlab::parse(""));
    if (empty.pc() != 0 || !empty.terminated() || !empty.program().instructions.empty()) return 1;
    try { empty.currentInstruction(); return 1; }
    catch (const std::out_of_range&) {}

    const auto sourceProgram = simtlab::parse("EXIT\nEXIT");
    simtlab::ExecutionState copiedProgram(sourceProgram);
    if (sourceProgram.instructions.size() != 2 || copiedProgram.program().instructions.size() != 2) return 1;
    simtlab::ExecutionState copiedState(copiedProgram);
    copiedState.advanceProgramCounter();
    if (copiedState.pc() != 1 || copiedProgram.pc() != 0) return 1;

    simtlab::ExecutionState moveSource(simtlab::parse("entry:\nEXIT\nEXIT"));
    moveSource.advanceProgramCounter();
    moveSource.warp().lanes[0].general_purpose_registers[0] = 7;
    simtlab::ExecutionState moved(std::move(moveSource));
    if (moved.pc() != 1 || moved.terminated() || moved.program().labels.at("entry") != 0
        || moved.warp().lanes[0].general_purpose_registers[0] != 7
        || moveSource.pc() != 0 || !moveSource.terminated() || !moveSource.program().instructions.empty()
        || !moveSource.program().labels.empty()
        || moveSource.warp().lanes[0].general_purpose_registers[0] != 0) return 1;
    try { moveSource.currentInstruction(); return 1; }
    catch (const std::out_of_range&) {}
    try { moveSource.advanceProgramCounter(); return 1; }
    catch (const std::out_of_range&) {}

    simtlab::ExecutionState assignmentSource(simtlab::parse("start:\nEXIT\nEXIT"));
    assignmentSource.advanceProgramCounter();
    simtlab::ExecutionState assigned(simtlab::parse("EXIT"));
    assigned = std::move(assignmentSource);
    if (assigned.pc() != 1 || assigned.terminated() || assigned.program().instructions.size() != 2
        || assigned.program().labels.at("start") != 0
        || assignmentSource.pc() != 0 || !assignmentSource.terminated()
        || !assignmentSource.program().instructions.empty() || !assignmentSource.program().labels.empty()) return 1;
    return 0;
}
