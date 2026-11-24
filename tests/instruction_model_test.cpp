#include <simtlab/isa/instruction.hpp>
#include <simtlab/isa/lane_state.hpp>
#include <simtlab/isa/warp.hpp>

#include <cstdint>
#include <variant>

int main()
{
    using namespace simtlab;

    Instruction instruction{Opcode::ADD, {GprIndex::R1, std::int32_t{7}}, std::nullopt};
    if (instruction.opcode != Opcode::ADD || instruction.operands.size() != 2
        || std::get<GprIndex>(instruction.operands[0]) != GprIndex::R1
        || std::get<std::int32_t>(instruction.operands[1]) != 7
        || instruction.predicate)
    {
        return 1;
    }

    instruction.predicate = Predicate{PredicateIndex::P2, false};
    if (!instruction.predicate || instruction.predicate->index != PredicateIndex::P2
        || instruction.predicate->negated)
    {
        return 1;
    }
    instruction.predicate = Predicate{PredicateIndex::P3, true};
    if (instruction.predicate->index != PredicateIndex::P3 || !instruction.predicate->negated)
    {
        return 1;
    }

    LaneState lane{};
    if (lane.general_purpose_registers.size() != GPR_COUNT
        || lane.predicate_registers.size() != PREDICATE_COUNT)
    {
        return 1;
    }
    for (const auto value : lane.general_purpose_registers)
    {
        if (value != 0) return 1;
    }
    for (const auto value : lane.predicate_registers)
    {
        if (value) return 1;
    }
    lane.general_purpose_registers[0] = 9;
    lane.predicate_registers[0] = true;
    if (lane.general_purpose_registers[1] != 0 || lane.predicate_registers[1]) return 1;

    static_assert(WARP_SIZE == 32);
    Warp warp{};
    if (warp.lanes.size() != WARP_SIZE) return 1;
    for (const auto& warp_lane : warp.lanes)
    {
        for (const auto value : warp_lane.general_purpose_registers)
        {
            if (value != 0) return 1;
        }
        for (const auto value : warp_lane.predicate_registers)
        {
            if (value) return 1;
        }
    }
    warp.lanes[0].general_purpose_registers[0] = 11;
    if (warp.lanes[1].general_purpose_registers[0] != 0) return 1;

    return 0;
}
