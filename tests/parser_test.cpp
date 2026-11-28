#include <simtlab/isa/parser.hpp>
#include <simtlab/isa/register.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>

bool rejects(const char* source, const char* location)
{
    try { simtlab::parse(source); } catch (const std::runtime_error& error)
    { return std::string(error.what()).find(location) != std::string::npos; }
    return false;
}

int main()
{
    const auto empty = simtlab::parse("");
    const auto blank = simtlab::parse("\n # comment\r\n\t\n");
    const auto exits = simtlab::parse("EXIT\nexit");
    if (!empty.instructions.empty() || !empty.labels.empty() || !blank.instructions.empty() || !blank.labels.empty()
        || exits.instructions.size() != 2 || exits.instructions[0].opcode != simtlab::Opcode::EXIT
        || !exits.instructions[0].operands.empty() || exits.instructions[1].opcode != simtlab::Opcode::EXIT) return 1;
    if (!rejects("NOPE", "1:1") || !rejects("EXIT r0", "1:6") || !rejects("@EXIT", "1:2")
        || !rejects("\n  nope", "2:3")) return 1;
    const auto predicated = simtlab::parse("@p0 ADD r0, r1, r2\n@!P3 setp.eq p2, r4, r5\nEXIT");
    if (predicated.instructions.size() != 3
        || !predicated.instructions[0].predicate
        || predicated.instructions[0].predicate->index != simtlab::PredicateIndex::P0
        || predicated.instructions[0].predicate->negated
        || !predicated.instructions[1].predicate
        || predicated.instructions[1].predicate->index != simtlab::PredicateIndex::P3
        || !predicated.instructions[1].predicate->negated
        || predicated.instructions[2].predicate) return 1;
    const auto spacedPredicate = simtlab::parse("@ ! p3  add r0, r1, r2");
    if (!spacedPredicate.instructions[0].predicate || !spacedPredicate.instructions[0].predicate->negated
        || spacedPredicate.instructions[0].predicate->index != simtlab::PredicateIndex::P3) return 1;
    if (!rejects("@", "1:2") || !rejects("@!", "1:3") || !rejects("@p", "1:2")
        || !rejects("@p4 ADD r0, r1, r2", "1:2") || !rejects("@p32 ADD r0, r1, r2", "1:2")
        || !rejects("@x ADD r0, r1, r2", "1:2") || !rejects("@p0", "1:1")
        || !rejects("@p0 label:", "1:5") || !rejects("@p0 MMA.M16N16K16 r0, r8, r16", "1:5")) return 1;
    const auto labels = simtlab::parse("first:\nsecond:\nEXIT\nend:");
    if (labels.instructions.size() != 1 || labels.instructions[0].opcode != simtlab::Opcode::EXIT
        || labels.labels.size() != 3 || labels.labels.at("first") != 0
        || labels.labels.at("second") != 0 || labels.labels.at("end") != 1) return 1;
    const auto case_sensitive = simtlab::parse("Name:\nname:");
    if (case_sensitive.labels.size() != 2 || case_sensitive.labels.at("Name") != 0
        || case_sensitive.labels.at("name") != 0) return 1;
    if (!rejects("same:\nEXIT\nsame:", "3:1") || !rejects("label: EXIT", "1:8")) return 1;
    bool duplicate_reported = false;
    try { simtlab::parse("dup:\nEXIT\ndup:"); }
    catch (const std::runtime_error& error)
    {
        duplicate_reported = std::string(error.what()).find("duplicate label at 3:1") != std::string::npos;
    }
    if (!duplicate_reported) return 1;
    const auto branches = simtlab::parse("bRa forward_target, reconv\nforward_target:\nEXIT\nreconv:\nEXIT");
    if (branches.instructions.size() != 3 || branches.instructions[0].opcode != simtlab::Opcode::BRA
        || branches.instructions[0].operands.size() != 2
        || !std::holds_alternative<simtlab::LabelOperand>(branches.instructions[0].operands[0])
        || !std::holds_alternative<simtlab::LabelOperand>(branches.instructions[0].operands[1])
        || std::get<simtlab::LabelOperand>(branches.instructions[0].operands[0]).name != "forward_target"
        || std::get<simtlab::LabelOperand>(branches.instructions[0].operands[1]).name != "reconv"
        || branches.labels.at("forward_target") != 1 || branches.labels.at("reconv") != 2) return 1;
    const auto backward_branch = simtlab::parse("begin:\nEXIT\nBRA begin, begin");
    if (backward_branch.instructions.size() != 2 || backward_branch.instructions[1].opcode != simtlab::Opcode::BRA
        || std::get<simtlab::LabelOperand>(backward_branch.instructions[1].operands[0]).name != "begin"
        || std::get<simtlab::LabelOperand>(backward_branch.instructions[1].operands[1]).name != "begin") return 1;
    if (!rejects("BRA", "1:4") || !rejects("BRA target", "1:11")
        || !rejects("BRA target,", "1:12") || !rejects("BRA 4, reconv", "1:5")
        || !rejects("BRA target, 4", "1:13") || !rejects("BRA target reconv", "1:12")
        || !rejects("BRA target, reconv, extra", "1:19") || !rejects("BRA @target, reconv", "1:5")) return 1;
    const auto jumps = simtlab::parse("jMp forward_target\nforward_target:\nEXIT");
    if (jumps.instructions.size() != 2 || jumps.instructions[0].opcode != simtlab::Opcode::JMP
        || jumps.instructions[0].operands.size() != 1
        || !std::holds_alternative<simtlab::LabelOperand>(jumps.instructions[0].operands[0])
        || std::get<simtlab::LabelOperand>(jumps.instructions[0].operands[0]).name != "forward_target"
        || jumps.labels.at("forward_target") != 1) return 1;
    const auto backward_jump = simtlab::parse("start:\nEXIT\nJMP start");
    if (backward_jump.instructions.size() != 2 || backward_jump.instructions[1].opcode != simtlab::Opcode::JMP
        || backward_jump.instructions[1].operands.size() != 1
        || std::get<simtlab::LabelOperand>(backward_jump.instructions[1].operands[0]).name != "start") return 1;
    if (!rejects("JMP", "1:4") || !rejects("JMP target extra", "1:12")
        || !rejects("JMP 3", "1:5") || !rejects("JMP @target", "1:5")
        || !rejects("JMP target, other", "1:11")) return 1;
    const auto multipleControlRefs = simtlab::parse("entry:\nBRA body, done\nbody:\nJMP done\ndone:\nEXIT");
    if (multipleControlRefs.instructions.size() != 3 || multipleControlRefs.labels.size() != 3
        || multipleControlRefs.labels.at("entry") != 0 || multipleControlRefs.labels.at("body") != 1
        || multipleControlRefs.labels.at("done") != 2) return 1;
    auto reports = [](const char* source, const char* message)
    {
        try { simtlab::parse(source); }
        catch (const std::runtime_error& error) { return std::string(error.what()).find(message) != std::string::npos; }
        return false;
    };
    if (!reports("BRA missing, reconv\nreconv:\nEXIT",
                 "undefined BRA branch target label 'missing' at 1:5")
        || !reports("target:\nBRA target, missing\nEXIT",
                    "undefined BRA reconvergence target label 'missing' at 2:13")
        || !reports("JMP missing", "undefined JMP target label 'missing' at 1:5")
        || !reports("BRA body, end\nbody:\nEXIT\nend:",
                    "BRA reconvergence target label 'end' must identify an instruction at 1:11")) return 1;
    const auto eofTarget = simtlab::parse("JMP end\nend:");
    if (eofTarget.instructions.size() != 1 || eofTarget.labels.at("end") != 1) return 1;
    const auto specialReads = simtlab::parse(
        "sreg r0, laneid\nSReG R31, wArPiD\nSREG r0, TID.X\nSREG r0, TID.Y\nSREG r0, TID.Z"
        "\nSREG r0, CTAID.X\nSREG r0, CTAID.Y\nSREG r0, CTAID.Z"
        "\nSREG r0, NTID.X\nSREG r0, NTID.Y\nSREG r0, NTID.Z"
        "\nSREG r0, GRIDID.X\nSREG r0, GRIDID.Y\nSREG r0, GRIDID.Z");
    const simtlab::SpecialRegister expectedSpecials[] = {
        simtlab::SpecialRegister::LANEID, simtlab::SpecialRegister::WARPID,
        simtlab::SpecialRegister::TID_X, simtlab::SpecialRegister::TID_Y, simtlab::SpecialRegister::TID_Z,
        simtlab::SpecialRegister::CTAID_X, simtlab::SpecialRegister::CTAID_Y, simtlab::SpecialRegister::CTAID_Z,
        simtlab::SpecialRegister::NTID_X, simtlab::SpecialRegister::NTID_Y, simtlab::SpecialRegister::NTID_Z,
        simtlab::SpecialRegister::GRIDID_X, simtlab::SpecialRegister::GRIDID_Y, simtlab::SpecialRegister::GRIDID_Z};
    if (specialReads.instructions.size() != 14) return 1;
    for (std::size_t i = 0; i < specialReads.instructions.size(); ++i)
    {
        const auto& instruction = specialReads.instructions[i];
        if (instruction.opcode != simtlab::Opcode::SREG || instruction.operands.size() != 2
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[0])
            || !std::holds_alternative<simtlab::SpecialRegister>(instruction.operands[1])
            || std::get<simtlab::SpecialRegister>(instruction.operands[1]) != expectedSpecials[i]) return 1;
    }
    if (std::get<simtlab::GprIndex>(specialReads.instructions[0].operands[0]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(specialReads.instructions[1].operands[0]) != simtlab::GprIndex::R31) return 1;
    if (!rejects("SREG", "1:5") || !rejects("SREG r0", "1:8")
        || !rejects("SREG r0,", "1:9") || !rejects("SREG p0, LANEID", "1:6")
        || !rejects("SREG r32, LANEID", "1:6") || !rejects("SREG rx, LANEID", "1:6")
        || !rejects("SREG r0, UNKNOWN", "1:10") || !rejects("SREG r0, TID", "1:13")
        || !rejects("SREG r0, TID X", "1:14") || !rejects("SREG r0, TID.W", "1:14")
        || !rejects("SREG r0, LANEID, r1", "1:16") || !rejects("SREG r0 LANEID", "1:9")) return 1;
    const auto mma = simtlab::parse("mma.m16n16k16 r0, r8, r16\nMMA.M16N16K16 R24, r0, r8");
    if (mma.instructions.size() != 2) return 1;
    for (const auto& instruction : mma.instructions)
        if (instruction.opcode != simtlab::Opcode::MMA_M16N16K16 || instruction.operands.size() != 3
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[0])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[1])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[2])) return 1;
    if (std::get<simtlab::GprIndex>(mma.instructions[0].operands[0]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(mma.instructions[0].operands[1]) != simtlab::GprIndex::R8
        || std::get<simtlab::GprIndex>(mma.instructions[0].operands[2]) != simtlab::GprIndex::R16
        || std::get<simtlab::GprIndex>(mma.instructions[1].operands[0]) != simtlab::GprIndex::R24
        || std::get<simtlab::GprIndex>(mma.instructions[1].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(mma.instructions[1].operands[2]) != simtlab::GprIndex::R8) return 1;
    if (!rejects("MMA.M16N16K16", "1:14") || !rejects("MMA.M16N16K16 r0", "1:17")
        || !rejects("MMA.M16N16K16 r0, r8", "1:21")
        || !rejects("MMA.M16N16K16 r0, r8, r16, r24", "1:26")
        || !rejects("MMA.M16N16K16 r0 r8, r16", "1:18")
        || !rejects("MMA.M16N16K16 r0, r8 r16", "1:22")
        || !rejects("MMA.M16N16K16 r32, r8, r16", "1:15")
        || !rejects("MMA.M16N16K16 rX, r8, r16", "1:15")
        || !rejects("MMA.M16N16K16 r25, r0, r8", "1:15")
        || !rejects("MMA.M16N16K16 r0, r7, r16", "1:19")
        || !rejects("MMA.M16N16K16 r0, r8, r12", "1:23")
        || !rejects("MMA.M16N16K8 r0, r8, r16", "1:1")) return 1;
    const auto barriers = simtlab::parse("BAR.SYNC 0\nbar.sync +4\nBaR.sYnC 0xF\nBAR.SYNC -0");
    if (barriers.instructions.size() != 4) return 1;
    for (const auto& instruction : barriers.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::BAR_SYNC
            || instruction.operands.size() != 1
            || !std::holds_alternative<std::uint32_t>(instruction.operands[0])) return 1;
    }
    if (std::get<std::uint32_t>(barriers.instructions[0].operands[0]) != 0
        || std::get<std::uint32_t>(barriers.instructions[1].operands[0]) != 4
        || std::get<std::uint32_t>(barriers.instructions[2].operands[0]) != 15
        || std::get<std::uint32_t>(barriers.instructions[3].operands[0]) != 0) return 1;
    if (!rejects("BAR.SYNC", "1:9") || !rejects("BAR.SYNC 16", "1:10")
        || !rejects("BAR.SYNC 0x10", "1:10") || !rejects("BAR.SYNC -1", "1:10")
        || !rejects("BAR.SYNC 1.5", "1:10") || !rejects("BAR.SYNC 1 2", "1:12")
        || !rejects("BAR.SYNC +", "1:11")) return 1;
    const auto adds = simtlab::parse("add r0, R31, r1\nAdD r31, r0, r31");
    if (adds.instructions.size() != 2) return 1;
    for (const auto& instruction : adds.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::ADD || instruction.operands.size() != 3) return 1;
        for (const auto& operand : instruction.operands)
            if (!std::holds_alternative<simtlab::GprIndex>(operand)) return 1;
    }
    if (std::get<simtlab::GprIndex>(adds.instructions[0].operands[0]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(adds.instructions[0].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(adds.instructions[0].operands[2]) != simtlab::GprIndex::R1
        || std::get<simtlab::GprIndex>(adds.instructions[1].operands[0]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(adds.instructions[1].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(adds.instructions[1].operands[2]) != simtlab::GprIndex::R31) return 1;
    if (!rejects("ADD", "1:4") || !rejects("ADD r0, r1,", "1:12")
        || !rejects("ADD r0, r1, r2, r3", "1:15") || !rejects("ADD r0 r1, r2", "1:8")
        || !rejects("ADD ,r0,r1", "1:5") || !rejects("ADD r0,,r1", "1:8")
        || !rejects("ADD rx, r1, r2", "1:5") || !rejects("ADD r32, r1, r2", "1:5")) return 1;
    const auto addImmediates = simtlab::parse(
        "ADD r2, r3, 7\nADD r2, r3, -1\nADD r2, r3, 0x10\nADD r2, r3, -0x2\n"
        "ADD r2, r3, 4294967295\nADD r2, r3, +0\nADD r2, r3, 0xFFFFFFFF\n"
        "ADD r2, r3, -4294967295");
    const std::uint32_t addImmediateValues[] = {7, 0xffffffffu, 16, 0xfffffffeu,
                                                0xffffffffu, 0, 0xffffffffu, 1};
    if (addImmediates.instructions.size() != 8) return 1;
    for (std::size_t i = 0; i < addImmediates.instructions.size(); ++i)
    {
        const auto& instruction = addImmediates.instructions[i];
        if (instruction.opcode != simtlab::Opcode::ADD || instruction.operands.size() != 3
            || !std::holds_alternative<std::uint32_t>(instruction.operands[2])
            || std::get<std::uint32_t>(instruction.operands[2]) != addImmediateValues[i]) return 1;
    }
    if (!rejects("ADD r0, r1, 4294967296", "1:")
        || !rejects("ADD r0, r1, -4294967296", "1:")
        || !rejects("ADD r0, r1, 0x100000000", "1:")
        || !rejects("ADD r0, r1, 1.5", "1:")) return 1;
    const auto subs = simtlab::parse("SUB r0, r31, R0\nsUb R31, r0, r31");
    if (subs.instructions.size() != 2) return 1;
    for (const auto& instruction : subs.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::SUB || instruction.operands.size() != 3) return 1;
        for (const auto& operand : instruction.operands)
            if (!std::holds_alternative<simtlab::GprIndex>(operand)) return 1;
    }
    if (std::get<simtlab::GprIndex>(subs.instructions[0].operands[0]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(subs.instructions[0].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(subs.instructions[0].operands[2]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(subs.instructions[1].operands[0]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(subs.instructions[1].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(subs.instructions[1].operands[2]) != simtlab::GprIndex::R31) return 1;
    if (!rejects("SUB", "1:4") || !rejects("SUB r0, r1,", "1:12")
        || !rejects("SUB r0, r1, r2, r3", "1:15") || !rejects("SUB r0 r1, r2", "1:8")
        || !rejects("SUB ,r0,r1", "1:5") || !rejects("SUB r0,,r1", "1:8")
        || !rejects("SUB rx, r1, r2", "1:5") || !rejects("SUB r32, r1, r2", "1:5")) return 1;
    const auto subImmediates = simtlab::parse(
        "SUB r2, r3, 7\nSUB r2, r3, -1\nSUB r2, r3, 0x10\nSUB r2, r3, -0x2\n"
        "SUB r2, r3, 4294967295");
    const std::uint32_t subImmediateValues[] = {7, 0xffffffffu, 16, 0xfffffffeu, 0xffffffffu};
    if (subImmediates.instructions.size() != 5) return 1;
    for (std::size_t i = 0; i < subImmediates.instructions.size(); ++i)
    {
        const auto& instruction = subImmediates.instructions[i];
        if (instruction.opcode != simtlab::Opcode::SUB || instruction.operands.size() != 3
            || !std::holds_alternative<std::uint32_t>(instruction.operands[2])
            || std::get<std::uint32_t>(instruction.operands[2]) != subImmediateValues[i]) return 1;
    }
    if (!rejects("SUB r0, r1, 4294967296", "1:")
        || !rejects("SUB r0, r1, -4294967296", "1:")
        || !rejects("SUB r0, r1, 0x100000000", "1:")
        || !rejects("SUB r0, r1, 1.5", "1:")) return 1;
    const auto muls = simtlab::parse("MUL r0, r31, R0\nmUl R31, r0, r31");
    if (muls.instructions.size() != 2) return 1;
    for (const auto& instruction : muls.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::MUL || instruction.operands.size() != 3) return 1;
        for (const auto& operand : instruction.operands)
            if (!std::holds_alternative<simtlab::GprIndex>(operand)) return 1;
    }
    if (std::get<simtlab::GprIndex>(muls.instructions[0].operands[0]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(muls.instructions[0].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(muls.instructions[0].operands[2]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(muls.instructions[1].operands[0]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(muls.instructions[1].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(muls.instructions[1].operands[2]) != simtlab::GprIndex::R31) return 1;
    if (!rejects("MUL", "1:4") || !rejects("MUL r0, r1,", "1:12")
        || !rejects("MUL r0, r1, r2, r3", "1:15") || !rejects("MUL r0 r1, r2", "1:8")
        || !rejects("MUL ,r0,r1", "1:5") || !rejects("MUL r0,,r1", "1:8")
        || !rejects("MUL rx, r1, r2", "1:5") || !rejects("MUL r32, r1, r2", "1:5")
        || !rejects("MUL r0, r1, 2", "1:13")) return 1;
    const auto fmas = simtlab::parse("FMA r0, r1, r31, R0\nfMa R31, r0, r31, r1");
    if (fmas.instructions.size() != 2) return 1;
    for (const auto& instruction : fmas.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::FMA || instruction.operands.size() != 4) return 1;
        for (const auto& operand : instruction.operands)
            if (!std::holds_alternative<simtlab::GprIndex>(operand)) return 1;
    }
    if (std::get<simtlab::GprIndex>(fmas.instructions[0].operands[0]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(fmas.instructions[0].operands[1]) != simtlab::GprIndex::R1
        || std::get<simtlab::GprIndex>(fmas.instructions[0].operands[2]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(fmas.instructions[0].operands[3]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(fmas.instructions[1].operands[0]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(fmas.instructions[1].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(fmas.instructions[1].operands[2]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(fmas.instructions[1].operands[3]) != simtlab::GprIndex::R1) return 1;
    if (!rejects("FMA", "1:4") || !rejects("FMA r0, r1, r2,", "1:16")
        || !rejects("FMA r0, r1, r2, r3, r4", "1:19") || !rejects("FMA r0 r1, r2, r3", "1:8")
        || !rejects("FMA r0,,r1,r2", "1:8") || !rejects("FMA r0,r1,r2 r3", "1:14")
        || !rejects("FMA r0, r1, rx, r3", "1:13") || !rejects("FMA r0, r1, r32, r3", "1:13")
        || !rejects("FMA r0, r1, r2, 3", "1:17")) return 1;
    const auto ands = simtlab::parse("AND r0, r31, R0\naNd R31, r0, 0xFF\nAND r1, r2, -1"
                                     "\nAND r0, r1, -0x1\nAND r0, r1, 4294967295");
    if (ands.instructions.size() != 5) return 1;
    for (const auto& instruction : ands.instructions)
        if (instruction.opcode != simtlab::Opcode::AND || instruction.operands.size() != 3) return 1;
    if (!std::holds_alternative<simtlab::GprIndex>(ands.instructions[0].operands[2])
        || std::get<simtlab::GprIndex>(ands.instructions[0].operands[0]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(ands.instructions[0].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(ands.instructions[0].operands[2]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(ands.instructions[1].operands[0]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(ands.instructions[1].operands[1]) != simtlab::GprIndex::R0
        || !std::holds_alternative<std::uint32_t>(ands.instructions[1].operands[2])
        || std::get<std::uint32_t>(ands.instructions[1].operands[2]) != 255
        || std::get<std::uint32_t>(ands.instructions[2].operands[2]) != 0xFFFFFFFFu
        || std::get<std::uint32_t>(ands.instructions[3].operands[2]) != 0xFFFFFFFFu
        || std::get<std::uint32_t>(ands.instructions[4].operands[2]) != 0xFFFFFFFFu) return 1;
    if (!rejects("AND", "1:4") || !rejects("AND r0, r1,", "1:12")
        || !rejects("AND r0, r1, r2, r3", "1:15") || !rejects("AND r0 r1, r2", "1:8")
        || !rejects("AND r0,,r1", "1:8") || !rejects("AND rx, r1, r2", "1:5")
        || !rejects("AND r32, r1, r2", "1:5") || !rejects("AND r0, r1, 1.5", "1:13")
        || !rejects("AND r0, r1, 4294967296", "1:13")) return 1;
    const auto ors = simtlab::parse("OR r0, r31, R0\nor R31, r0, r31\nOr r1, r2, 0"
                                    "\nOR r0, r1, 4294967295\nOR r0, r1, -0x1");
    if (ors.instructions.size() != 5) return 1;
    for (const auto& instruction : ors.instructions)
        if (instruction.opcode != simtlab::Opcode::OR || instruction.operands.size() != 3) return 1;
    if (!std::holds_alternative<simtlab::GprIndex>(ors.instructions[0].operands[2])
        || std::get<simtlab::GprIndex>(ors.instructions[0].operands[0]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(ors.instructions[0].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(ors.instructions[0].operands[2]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(ors.instructions[1].operands[0]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(ors.instructions[1].operands[1]) != simtlab::GprIndex::R0
        || !std::holds_alternative<std::uint32_t>(ors.instructions[2].operands[2])
        || std::get<std::uint32_t>(ors.instructions[2].operands[2]) != 0
        || std::get<std::uint32_t>(ors.instructions[3].operands[2]) != 0xFFFFFFFFu
        || std::get<std::uint32_t>(ors.instructions[4].operands[2]) != 0xFFFFFFFFu) return 1;
    if (!rejects("OR", "1:3") || !rejects("OR r0, r1,", "1:11")
        || !rejects("OR r0, r1, r2, r3", "1:14") || !rejects("OR r0 r1, r2", "1:7")
        || !rejects("OR r0,,r1", "1:7") || !rejects("OR rx, r1, r2", "1:4")
        || !rejects("OR r32, r1, r2", "1:4") || !rejects("OR r0, r1, 1.5", "1:12")) return 1;
    const auto shls = simtlab::parse("SHL r0, r31, R0\nshl R31, r0, r31\nShL r1, r2, 0"
                                    "\nSHL r1, r2, 31\nSHL r1, r2, 4294967295\nSHL r1, r2, -0x1");
    if (shls.instructions.size() != 6) return 1;
    for (const auto& instruction : shls.instructions)
        if (instruction.opcode != simtlab::Opcode::SHL || instruction.operands.size() != 3) return 1;
    if (!std::holds_alternative<simtlab::GprIndex>(shls.instructions[0].operands[2])
        || std::get<simtlab::GprIndex>(shls.instructions[0].operands[0]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(shls.instructions[0].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(shls.instructions[0].operands[2]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(shls.instructions[1].operands[0]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(shls.instructions[1].operands[1]) != simtlab::GprIndex::R0
        || !std::holds_alternative<std::uint32_t>(shls.instructions[2].operands[2])
        || std::get<std::uint32_t>(shls.instructions[2].operands[2]) != 0
        || std::get<std::uint32_t>(shls.instructions[3].operands[2]) != 31
        || std::get<std::uint32_t>(shls.instructions[4].operands[2]) != 0xFFFFFFFFu
        || std::get<std::uint32_t>(shls.instructions[5].operands[2]) != 0xFFFFFFFFu) return 1;
    if (!rejects("SHL", "1:4") || !rejects("SHL r0, r1,", "1:12")
        || !rejects("SHL r0, r1, r2, r3", "1:15") || !rejects("SHL r0 r1, r2", "1:8")
        || !rejects("SHL r0,,r1", "1:8") || !rejects("SHL rx, r1, r2", "1:5")
        || !rejects("SHL r32, r1, r2", "1:5") || !rejects("SHL r0, r1, 1.5", "1:13")
        || !rejects("SHL r0, r1, 4294967296", "1:13")) return 1;
    const auto shrs = simtlab::parse("SHR r0, r31, R0\nshr R31, r0, r31\nShR r1, r2, 0"
                                    "\nSHR r1, r2, 31\nSHR r1, r2, 4294967295\nSHR r1, r2, -0x1");
    if (shrs.instructions.size() != 6) return 1;
    for (const auto& instruction : shrs.instructions)
        if (instruction.opcode != simtlab::Opcode::SHR || instruction.operands.size() != 3) return 1;
    if (!std::holds_alternative<simtlab::GprIndex>(shrs.instructions[0].operands[2])
        || std::get<simtlab::GprIndex>(shrs.instructions[0].operands[0]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(shrs.instructions[0].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(shrs.instructions[0].operands[2]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(shrs.instructions[1].operands[0]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(shrs.instructions[1].operands[1]) != simtlab::GprIndex::R0
        || !std::holds_alternative<std::uint32_t>(shrs.instructions[2].operands[2])
        || std::get<std::uint32_t>(shrs.instructions[2].operands[2]) != 0
        || std::get<std::uint32_t>(shrs.instructions[3].operands[2]) != 31
        || std::get<std::uint32_t>(shrs.instructions[4].operands[2]) != 0xFFFFFFFFu
        || std::get<std::uint32_t>(shrs.instructions[5].operands[2]) != 0xFFFFFFFFu) return 1;
    if (!rejects("SHR", "1:4") || !rejects("SHR r0, r1,", "1:12")
        || !rejects("SHR r0, r1, r2, r3", "1:15") || !rejects("SHR r0 r1, r2", "1:8")
        || !rejects("SHR r0,,r1", "1:8") || !rejects("SHR rx, r1, r2", "1:5")
        || !rejects("SHR r32, r1, r2", "1:5") || !rejects("SHR r0, r1, 1.5", "1:13")
        || !rejects("SHR r0, r1, 4294967296", "1:13")) return 1;
    const auto fadds = simtlab::parse("FADD r0, R31, r0\nfAdD R31, r0, r31");
    if (fadds.instructions.size() != 2) return 1;
    for (const auto& instruction : fadds.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::FADD || instruction.operands.size() != 3) return 1;
        for (const auto& operand : instruction.operands)
            if (!std::holds_alternative<simtlab::GprIndex>(operand)) return 1;
    }
    if (std::get<simtlab::GprIndex>(fadds.instructions[0].operands[0]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(fadds.instructions[0].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(fadds.instructions[0].operands[2]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(fadds.instructions[1].operands[0]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(fadds.instructions[1].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(fadds.instructions[1].operands[2]) != simtlab::GprIndex::R31) return 1;
    if (!rejects("FADD", "1:5") || !rejects("FADD r0, r1,", "1:13")
        || !rejects("FADD r0, r1, r2, r3", "1:16") || !rejects("FADD r0 r1, r2", "1:9")
        || !rejects("FADD r0,,r1", "1:9") || !rejects("FADD rx, r1, r2", "1:6")
        || !rejects("FADD r32, r1, r2", "1:6") || !rejects("FADD r0, r1, 2", "1:14")
        || !rejects("FADD r0, r1, 1.5", "1:14")) return 1;
    const auto fmuls = simtlab::parse("FMUL r0, R31, r0\nfMuL R31, r0, r31");
    if (fmuls.instructions.size() != 2) return 1;
    for (const auto& instruction : fmuls.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::FMUL || instruction.operands.size() != 3) return 1;
        for (const auto& operand : instruction.operands)
            if (!std::holds_alternative<simtlab::GprIndex>(operand)) return 1;
    }
    if (std::get<simtlab::GprIndex>(fmuls.instructions[0].operands[0]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(fmuls.instructions[0].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(fmuls.instructions[0].operands[2]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(fmuls.instructions[1].operands[0]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(fmuls.instructions[1].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(fmuls.instructions[1].operands[2]) != simtlab::GprIndex::R31) return 1;
    if (!rejects("FMUL", "1:5") || !rejects("FMUL r0, r1,", "1:13")
        || !rejects("FMUL r0, r1, r2, r3", "1:16") || !rejects("FMUL r0 r1, r2", "1:9")
        || !rejects("FMUL r0,,r1", "1:9") || !rejects("FMUL rx, r1, r2", "1:6")
        || !rejects("FMUL r32, r1, r2", "1:6") || !rejects("FMUL r0, r1, 2", "1:14")
        || !rejects("FMUL r0, r1, 1.5", "1:14")) return 1;
    const auto setpEq = simtlab::parse("SETP.EQ p0, r0, R31\nsetp.eq P3, r31, r0");
    if (setpEq.instructions.size() != 2) return 1;
    for (const auto& instruction : setpEq.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::SETP_EQ || instruction.operands.size() != 3) return 1;
        if (!std::holds_alternative<simtlab::PredicateIndex>(instruction.operands[0])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[1])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[2])) return 1;
    }
    if (std::get<simtlab::PredicateIndex>(setpEq.instructions[0].operands[0]) != simtlab::PredicateIndex::P0
        || std::get<simtlab::GprIndex>(setpEq.instructions[0].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(setpEq.instructions[0].operands[2]) != simtlab::GprIndex::R31
        || std::get<simtlab::PredicateIndex>(setpEq.instructions[1].operands[0]) != simtlab::PredicateIndex::P3
        || std::get<simtlab::GprIndex>(setpEq.instructions[1].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(setpEq.instructions[1].operands[2]) != simtlab::GprIndex::R0) return 1;
    if (!rejects("SETP.EQ", "1:8") || !rejects("SETP.EQ p0, r0,", "1:16")
        || !rejects("SETP.EQ p0, r0, r1, r2", "1:19") || !rejects("SETP.EQ p0 r0, r1", "1:12")
        || !rejects("SETP.EQ p0,,r0,r1", "1:12") || !rejects("SETP.EQ p4, r0, r1", "1:9")
        || !rejects("SETP.EQ px, r0, r1", "1:9") || !rejects("SETP.EQ p0, r32, r1", "1:13")
        || !rejects("SETP.EQ p0, r0, 1", "1:17") || !rejects("SETP.EQ p0, r0, 1.5", "1:17")) return 1;
    const auto setpNe = simtlab::parse("SETP.NE p0, r0, R31\nsetp.ne P3, r31, r0");
    if (setpNe.instructions.size() != 2) return 1;
    for (const auto& instruction : setpNe.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::SETP_NE || instruction.operands.size() != 3) return 1;
        if (!std::holds_alternative<simtlab::PredicateIndex>(instruction.operands[0])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[1])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[2])) return 1;
    }
    if (std::get<simtlab::PredicateIndex>(setpNe.instructions[0].operands[0]) != simtlab::PredicateIndex::P0
        || std::get<simtlab::GprIndex>(setpNe.instructions[0].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(setpNe.instructions[0].operands[2]) != simtlab::GprIndex::R31
        || std::get<simtlab::PredicateIndex>(setpNe.instructions[1].operands[0]) != simtlab::PredicateIndex::P3
        || std::get<simtlab::GprIndex>(setpNe.instructions[1].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(setpNe.instructions[1].operands[2]) != simtlab::GprIndex::R0) return 1;
    if (!rejects("SETP.NE", "1:8") || !rejects("SETP.NE p0, r0,", "1:16")
        || !rejects("SETP.NE p0, r0, r1, r2", "1:19") || !rejects("SETP.NE p0 r0, r1", "1:12")
        || !rejects("SETP.NE p0,,r0,r1", "1:12") || !rejects("SETP.NE p4, r0, r1", "1:9")
        || !rejects("SETP.NE px, r0, r1", "1:9") || !rejects("SETP.NE p0, rx, r1", "1:13")
        || !rejects("SETP.NE p0, r32, r1", "1:13") || !rejects("SETP.NE p0, r0, 1", "1:17")
        || !rejects("SETP.NE p0, r0, 1.5", "1:17")) return 1;
    const auto setpLt = simtlab::parse("SETP.LT p0, r0, R31\nsEtP.lT P3, r31, r0");
    if (setpLt.instructions.size() != 2) return 1;
    for (const auto& instruction : setpLt.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::SETP_LT || instruction.operands.size() != 3) return 1;
        if (!std::holds_alternative<simtlab::PredicateIndex>(instruction.operands[0])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[1])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[2])) return 1;
    }
    if (std::get<simtlab::PredicateIndex>(setpLt.instructions[0].operands[0]) != simtlab::PredicateIndex::P0
        || std::get<simtlab::GprIndex>(setpLt.instructions[0].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(setpLt.instructions[0].operands[2]) != simtlab::GprIndex::R31
        || std::get<simtlab::PredicateIndex>(setpLt.instructions[1].operands[0]) != simtlab::PredicateIndex::P3
        || std::get<simtlab::GprIndex>(setpLt.instructions[1].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(setpLt.instructions[1].operands[2]) != simtlab::GprIndex::R0) return 1;
    if (!rejects("SETP.LT", "1:8") || !rejects("SETP.LT p0, r0,", "1:16")
        || !rejects("SETP.LT p0, r0, r1, r2", "1:19") || !rejects("SETP.LT p0 r0, r1", "1:12")
        || !rejects("SETP.LT p0,,r0,r1", "1:12") || !rejects("SETP.LT p4, r0, r1", "1:9")
        || !rejects("SETP.LT px, r0, r1", "1:9") || !rejects("SETP.LT p0, rx, r1", "1:13")
        || !rejects("SETP.LT p0, r32, r1", "1:13") || !rejects("SETP.LT p0, r0, 1", "1:17")
        || !rejects("SETP.LT p0, r0, 1.5", "1:17")) return 1;
    const auto setpLe = simtlab::parse("SETP.LE p0, r0, R31\nsEtP.lE P3, r31, r0");
    if (setpLe.instructions.size() != 2) return 1;
    for (const auto& instruction : setpLe.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::SETP_LE || instruction.operands.size() != 3) return 1;
        if (!std::holds_alternative<simtlab::PredicateIndex>(instruction.operands[0])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[1])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[2])) return 1;
    }
    if (std::get<simtlab::PredicateIndex>(setpLe.instructions[0].operands[0]) != simtlab::PredicateIndex::P0
        || std::get<simtlab::GprIndex>(setpLe.instructions[0].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(setpLe.instructions[0].operands[2]) != simtlab::GprIndex::R31
        || std::get<simtlab::PredicateIndex>(setpLe.instructions[1].operands[0]) != simtlab::PredicateIndex::P3
        || std::get<simtlab::GprIndex>(setpLe.instructions[1].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(setpLe.instructions[1].operands[2]) != simtlab::GprIndex::R0) return 1;
    if (!rejects("SETP.LE", "1:8") || !rejects("SETP.LE p0, r0,", "1:16")
        || !rejects("SETP.LE p0, r0, r1, r2", "1:19") || !rejects("SETP.LE p0 r0, r1", "1:12")
        || !rejects("SETP.LE p0,,r0,r1", "1:12") || !rejects("SETP.LE p4, r0, r1", "1:9")
        || !rejects("SETP.LE px, r0, r1", "1:9") || !rejects("SETP.LE p0, rx, r1", "1:13")
        || !rejects("SETP.LE p0, r32, r1", "1:13") || !rejects("SETP.LE p0, r0, 1", "1:17")
        || !rejects("SETP.LE p0, r0, 1.5", "1:17")) return 1;
    const auto setpGt = simtlab::parse("SETP.GT p0, r0, R31\nsEtP.gT P3, r31, r0");
    if (setpGt.instructions.size() != 2) return 1;
    for (const auto& instruction : setpGt.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::SETP_GT || instruction.operands.size() != 3) return 1;
        if (!std::holds_alternative<simtlab::PredicateIndex>(instruction.operands[0])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[1])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[2])) return 1;
    }
    if (std::get<simtlab::PredicateIndex>(setpGt.instructions[0].operands[0]) != simtlab::PredicateIndex::P0
        || std::get<simtlab::GprIndex>(setpGt.instructions[0].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(setpGt.instructions[0].operands[2]) != simtlab::GprIndex::R31
        || std::get<simtlab::PredicateIndex>(setpGt.instructions[1].operands[0]) != simtlab::PredicateIndex::P3
        || std::get<simtlab::GprIndex>(setpGt.instructions[1].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(setpGt.instructions[1].operands[2]) != simtlab::GprIndex::R0) return 1;
    if (!rejects("SETP.GT", "1:8") || !rejects("SETP.GT p0, r0,", "1:16")
        || !rejects("SETP.GT p0, r0, r1, r2", "1:19") || !rejects("SETP.GT p0 r0, r1", "1:12")
        || !rejects("SETP.GT p0,,r0,r1", "1:12") || !rejects("SETP.GT p0, r0 r1", "1:16")
        || !rejects("SETP.GT p4, r0, r1", "1:9") || !rejects("SETP.GT px, r0, r1", "1:9")
        || !rejects("SETP.GT p0, rx, r1", "1:13") || !rejects("SETP.GT p0, r32, r1", "1:13")
        || !rejects("SETP.GT p0, r0, 1", "1:17") || !rejects("SETP.GT p0, r0, 1.5", "1:17")) return 1;
    const auto setpGe = simtlab::parse("SETP.GE p0, r0, R31\nsEtP.gE P3, r31, r0");
    if (setpGe.instructions.size() != 2) return 1;
    for (const auto& instruction : setpGe.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::SETP_GE || instruction.operands.size() != 3) return 1;
        if (!std::holds_alternative<simtlab::PredicateIndex>(instruction.operands[0])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[1])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[2])) return 1;
    }
    if (std::get<simtlab::PredicateIndex>(setpGe.instructions[0].operands[0]) != simtlab::PredicateIndex::P0
        || std::get<simtlab::GprIndex>(setpGe.instructions[0].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(setpGe.instructions[0].operands[2]) != simtlab::GprIndex::R31
        || std::get<simtlab::PredicateIndex>(setpGe.instructions[1].operands[0]) != simtlab::PredicateIndex::P3
        || std::get<simtlab::GprIndex>(setpGe.instructions[1].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(setpGe.instructions[1].operands[2]) != simtlab::GprIndex::R0) return 1;
    if (!rejects("SETP.GE", "1:8") || !rejects("SETP.GE p0, r0,", "1:16")
        || !rejects("SETP.GE p0, r0, r1, r2", "1:19") || !rejects("SETP.GE p0 r0, r1", "1:12")
        || !rejects("SETP.GE p0,,r0,r1", "1:12") || !rejects("SETP.GE p0, r0 r1", "1:16")
        || !rejects("SETP.GE p4, r0, r1", "1:9") || !rejects("SETP.GE px, r0, r1", "1:9")
        || !rejects("SETP.GE p0, rx, r1", "1:13") || !rejects("SETP.GE p0, r32, r1", "1:13")
        || !rejects("SETP.GE p0, r0, 1", "1:17") || !rejects("SETP.GE p0, r0, 1.5", "1:17")) return 1;
    const auto setpFeq = simtlab::parse("SETP.FEQ p0, r0, R31\nsEtP.fEq P3, r31, r0");
    if (setpFeq.instructions.size() != 2) return 1;
    for (const auto& instruction : setpFeq.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::SETP_FEQ || instruction.operands.size() != 3) return 1;
        if (!std::holds_alternative<simtlab::PredicateIndex>(instruction.operands[0])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[1])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[2])) return 1;
    }
    if (std::get<simtlab::PredicateIndex>(setpFeq.instructions[0].operands[0]) != simtlab::PredicateIndex::P0
        || std::get<simtlab::GprIndex>(setpFeq.instructions[0].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(setpFeq.instructions[0].operands[2]) != simtlab::GprIndex::R31
        || std::get<simtlab::PredicateIndex>(setpFeq.instructions[1].operands[0]) != simtlab::PredicateIndex::P3
        || std::get<simtlab::GprIndex>(setpFeq.instructions[1].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(setpFeq.instructions[1].operands[2]) != simtlab::GprIndex::R0) return 1;
    if (!rejects("SETP.FEQ", "1:9") || !rejects("SETP.FEQ p0, r0,", "1:17")
        || !rejects("SETP.FEQ p0, r0, r1, r2", "1:20") || !rejects("SETP.FEQ p0 r0, r1", "1:13")
        || !rejects("SETP.FEQ p0,,r0,r1", "1:13") || !rejects("SETP.FEQ p0, r0 r1", "1:17")
        || !rejects("SETP.FEQ p4, r0, r1", "1:10") || !rejects("SETP.FEQ px, r0, r1", "1:10")
        || !rejects("SETP.FEQ p0, rx, r1", "1:14") || !rejects("SETP.FEQ p0, r32, r1", "1:14")
        || !rejects("SETP.FEQ p0, r0, 1", "1:18") || !rejects("SETP.FEQ p0, r0, 1.5", "1:18")) return 1;
    const auto setpFne = simtlab::parse("SETP.FNE p0, r0, R31\nsEtP.fNe P3, r31, r0");
    if (setpFne.instructions.size() != 2) return 1;
    for (const auto& instruction : setpFne.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::SETP_FNE || instruction.operands.size() != 3) return 1;
        if (!std::holds_alternative<simtlab::PredicateIndex>(instruction.operands[0])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[1])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[2])) return 1;
    }
    if (std::get<simtlab::PredicateIndex>(setpFne.instructions[0].operands[0]) != simtlab::PredicateIndex::P0
        || std::get<simtlab::GprIndex>(setpFne.instructions[0].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(setpFne.instructions[0].operands[2]) != simtlab::GprIndex::R31
        || std::get<simtlab::PredicateIndex>(setpFne.instructions[1].operands[0]) != simtlab::PredicateIndex::P3
        || std::get<simtlab::GprIndex>(setpFne.instructions[1].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(setpFne.instructions[1].operands[2]) != simtlab::GprIndex::R0) return 1;
    if (!rejects("SETP.FNE", "1:9") || !rejects("SETP.FNE p0, r0,", "1:17")
        || !rejects("SETP.FNE p0, r0, r1, r2", "1:20") || !rejects("SETP.FNE p0 r0, r1", "1:13")
        || !rejects("SETP.FNE p0,,r0,r1", "1:13") || !rejects("SETP.FNE p0, r0 r1", "1:17")
        || !rejects("SETP.FNE p4, r0, r1", "1:10") || !rejects("SETP.FNE px, r0, r1", "1:10")
        || !rejects("SETP.FNE p0, rx, r1", "1:14") || !rejects("SETP.FNE p0, r32, r1", "1:14")
        || !rejects("SETP.FNE p0, r0, 1", "1:18") || !rejects("SETP.FNE p0, r0, 1.5", "1:18")) return 1;
    const auto setpFlt = simtlab::parse("SETP.FLT p0, r0, R31\nsEtP.fLt P3, r31, r0");
    if (setpFlt.instructions.size() != 2) return 1;
    for (const auto& instruction : setpFlt.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::SETP_FLT || instruction.operands.size() != 3) return 1;
        if (!std::holds_alternative<simtlab::PredicateIndex>(instruction.operands[0])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[1])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[2])) return 1;
    }
    if (std::get<simtlab::PredicateIndex>(setpFlt.instructions[0].operands[0]) != simtlab::PredicateIndex::P0
        || std::get<simtlab::GprIndex>(setpFlt.instructions[0].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(setpFlt.instructions[0].operands[2]) != simtlab::GprIndex::R31
        || std::get<simtlab::PredicateIndex>(setpFlt.instructions[1].operands[0]) != simtlab::PredicateIndex::P3
        || std::get<simtlab::GprIndex>(setpFlt.instructions[1].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(setpFlt.instructions[1].operands[2]) != simtlab::GprIndex::R0) return 1;
    if (!rejects("SETP.FLT", "1:9") || !rejects("SETP.FLT p0, r0,", "1:17")
        || !rejects("SETP.FLT p0, r0, r1, r2", "1:20") || !rejects("SETP.FLT p0 r0, r1", "1:13")
        || !rejects("SETP.FLT p0,,r0,r1", "1:13") || !rejects("SETP.FLT p0, r0 r1", "1:17")
        || !rejects("SETP.FLT p4, r0, r1", "1:10") || !rejects("SETP.FLT px, r0, r1", "1:10")
        || !rejects("SETP.FLT p0, rx, r1", "1:14") || !rejects("SETP.FLT p0, r32, r1", "1:14")
        || !rejects("SETP.FLT p0, r0, 1", "1:18") || !rejects("SETP.FLT p0, r0, 1.5", "1:18")) return 1;
    const auto setpFle = simtlab::parse("SETP.FLE p0, r0, R31\nsEtP.fLe P3, r31, r0");
    if (setpFle.instructions.size() != 2) return 1;
    for (const auto& instruction : setpFle.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::SETP_FLE || instruction.operands.size() != 3) return 1;
        if (!std::holds_alternative<simtlab::PredicateIndex>(instruction.operands[0])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[1])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[2])) return 1;
    }
    if (std::get<simtlab::PredicateIndex>(setpFle.instructions[0].operands[0]) != simtlab::PredicateIndex::P0
        || std::get<simtlab::GprIndex>(setpFle.instructions[0].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(setpFle.instructions[0].operands[2]) != simtlab::GprIndex::R31
        || std::get<simtlab::PredicateIndex>(setpFle.instructions[1].operands[0]) != simtlab::PredicateIndex::P3
        || std::get<simtlab::GprIndex>(setpFle.instructions[1].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(setpFle.instructions[1].operands[2]) != simtlab::GprIndex::R0) return 1;
    if (!rejects("SETP.FLE", "1:9") || !rejects("SETP.FLE p0, r0,", "1:17")
        || !rejects("SETP.FLE p0, r0, r1, r2", "1:20") || !rejects("SETP.FLE p0 r0, r1", "1:13")
        || !rejects("SETP.FLE p0,,r0,r1", "1:13") || !rejects("SETP.FLE p0, r0 r1", "1:17")
        || !rejects("SETP.FLE p4, r0, r1", "1:10") || !rejects("SETP.FLE px, r0, r1", "1:10")
        || !rejects("SETP.FLE p0, rx, r1", "1:14") || !rejects("SETP.FLE p0, r32, r1", "1:14")
        || !rejects("SETP.FLE p0, r0, 1", "1:18") || !rejects("SETP.FLE p0, r0, 1.5", "1:18")) return 1;
    const auto setpFgt = simtlab::parse("SETP.FGT p0, r0, R31\nsEtP.fGt P3, r31, r0");
    if (setpFgt.instructions.size() != 2) return 1;
    for (const auto& instruction : setpFgt.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::SETP_FGT || instruction.operands.size() != 3) return 1;
        if (!std::holds_alternative<simtlab::PredicateIndex>(instruction.operands[0])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[1])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[2])) return 1;
    }
    if (std::get<simtlab::PredicateIndex>(setpFgt.instructions[0].operands[0]) != simtlab::PredicateIndex::P0
        || std::get<simtlab::GprIndex>(setpFgt.instructions[0].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(setpFgt.instructions[0].operands[2]) != simtlab::GprIndex::R31
        || std::get<simtlab::PredicateIndex>(setpFgt.instructions[1].operands[0]) != simtlab::PredicateIndex::P3
        || std::get<simtlab::GprIndex>(setpFgt.instructions[1].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(setpFgt.instructions[1].operands[2]) != simtlab::GprIndex::R0) return 1;
    if (!rejects("SETP.FGT", "1:9") || !rejects("SETP.FGT p0, r0,", "1:17")
        || !rejects("SETP.FGT p0, r0, r1, r2", "1:20") || !rejects("SETP.FGT p0 r0, r1", "1:13")
        || !rejects("SETP.FGT p0,,r0,r1", "1:13") || !rejects("SETP.FGT p0, r0 r1", "1:17")
        || !rejects("SETP.FGT p4, r0, r1", "1:10") || !rejects("SETP.FGT px, r0, r1", "1:10")
        || !rejects("SETP.FGT p0, rx, r1", "1:14") || !rejects("SETP.FGT p0, r32, r1", "1:14")
        || !rejects("SETP.FGT p0, r0, 1", "1:18") || !rejects("SETP.FGT p0, r0, 1.5", "1:18")) return 1;
    const auto setpFge = simtlab::parse("SETP.FGE p0, r0, R31\nsEtP.fGe P3, r31, r0");
    if (setpFge.instructions.size() != 2) return 1;
    for (const auto& instruction : setpFge.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::SETP_FGE || instruction.operands.size() != 3) return 1;
        if (!std::holds_alternative<simtlab::PredicateIndex>(instruction.operands[0])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[1])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[2])) return 1;
    }
    if (std::get<simtlab::PredicateIndex>(setpFge.instructions[0].operands[0]) != simtlab::PredicateIndex::P0
        || std::get<simtlab::GprIndex>(setpFge.instructions[0].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::GprIndex>(setpFge.instructions[0].operands[2]) != simtlab::GprIndex::R31
        || std::get<simtlab::PredicateIndex>(setpFge.instructions[1].operands[0]) != simtlab::PredicateIndex::P3
        || std::get<simtlab::GprIndex>(setpFge.instructions[1].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::GprIndex>(setpFge.instructions[1].operands[2]) != simtlab::GprIndex::R0) return 1;
    if (!rejects("SETP.FGE", "1:9") || !rejects("SETP.FGE p0, r0,", "1:17")
        || !rejects("SETP.FGE p0, r0, r1, r2", "1:20") || !rejects("SETP.FGE p0 r0, r1", "1:13")
        || !rejects("SETP.FGE p0,,r0,r1", "1:13") || !rejects("SETP.FGE p0, r0 r1", "1:17")
        || !rejects("SETP.FGE p4, r0, r1", "1:10") || !rejects("SETP.FGE px, r0, r1", "1:10")
        || !rejects("SETP.FGE p0, rx, r1", "1:14") || !rejects("SETP.FGE p0, r32, r1", "1:14")
        || !rejects("SETP.FGE p0, r0, 1", "1:18") || !rejects("SETP.FGE p0, r0, 1.5", "1:18")) return 1;
    const auto ldgs = simtlab::parse("LDG r0, [r31], 1\nldg R31, [r0+4], 2\nLdG r1, [r2-0x10], 4"
                                     "\nLDG r2, [r3+r31], 4\nLDG r4, [r5+2147483647], 4"
                                     "\nLDG r6, [r7-0x80000000], 1");
    if (ldgs.instructions.size() != 6) return 1;
    for (const auto& instruction : ldgs.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::LDG || instruction.operands.size() != 3
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[0])
            || !std::holds_alternative<simtlab::MemoryOperand>(instruction.operands[1])
            || !std::holds_alternative<simtlab::MemoryWidth>(instruction.operands[2])) return 1;
    }
    if (std::get<simtlab::GprIndex>(ldgs.instructions[0].operands[0]) != simtlab::GprIndex::R0
        || std::get<simtlab::MemoryOperand>(ldgs.instructions[0].operands[1]).base != simtlab::GprIndex::R31
        || !std::holds_alternative<std::monostate>(std::get<simtlab::MemoryOperand>(ldgs.instructions[0].operands[1]).offset)
        || std::get<simtlab::MemoryWidth>(ldgs.instructions[0].operands[2]) != simtlab::MemoryWidth::BYTE
        || std::get<std::int32_t>(std::get<simtlab::MemoryOperand>(ldgs.instructions[1].operands[1]).offset) != 4
        || std::get<simtlab::MemoryWidth>(ldgs.instructions[1].operands[2]) != simtlab::MemoryWidth::HALFWORD
        || std::get<std::int32_t>(std::get<simtlab::MemoryOperand>(ldgs.instructions[2].operands[1]).offset) != -16
        || std::get<simtlab::GprIndex>(std::get<simtlab::MemoryOperand>(ldgs.instructions[3].operands[1]).offset) != simtlab::GprIndex::R31
        || std::get<std::int32_t>(std::get<simtlab::MemoryOperand>(ldgs.instructions[4].operands[1]).offset) != std::int32_t{2147483647}
        || std::get<std::int32_t>(std::get<simtlab::MemoryOperand>(ldgs.instructions[5].operands[1]).offset) != std::int32_t{-2147483647 - 1}
        || std::get<simtlab::MemoryWidth>(ldgs.instructions[5].operands[2]) != simtlab::MemoryWidth::BYTE) return 1;
    if (!rejects("LDG", "1:4") || !rejects("LDG r32, [r1], 4", "1:5")
        || !rejects("LDG rx, [r1], 4", "1:5") || !rejects("LDG r0", "1:7")
        || !rejects("LDG r0, r1, 4", "1:9")
        || !rejects("LDG r0, [r1, 4", "1:12") || !rejects("LDG r0, r1], 4", "1:9")
        || !rejects("LDG r0, [r32], 4", "1:10") || !rejects("LDG r0, [r1+r32], 4", "1:13")
        || !rejects("LDG r0, [r1-r2], 4", "1:13") || !rejects("LDG r0, [r1+1.5], 4", "1:13")
        || !rejects("LDG r0, [r1+2147483648], 4", "1:13") || !rejects("LDG r0, [r1-2147483649], 4", "1:13")
        || !rejects("LDG r0, [r1],", "1:14") || !rejects("LDG r0, [r1], 3", "1:15")
        || !rejects("LDG r0, [r1], 0", "1:15") || !rejects("LDG r0, [r1], 8", "1:15")
        || !rejects("LDG r0, [r1], 1.5", "1:15")
        || !rejects("LDG r0, [r1], 4, 1", "1:16") || !rejects("LDG r0 [r1], 4", "1:8")
        || !rejects("LDG r0, [r1] 4", "1:14") || !rejects("LDG r0, [r1+], 4", "1:13")) return 1;
    const auto stgs = simtlab::parse("STG [r31], r0, 1\nstg [r0+4], R31, 2\nStG [r2-0x10], r3, 4"
                                     "\nSTG [r3+r31], r2, 4\nSTG [r5+2147483647], r4, 4"
                                     "\nSTG [r7-0x80000000], r6, 1");
    if (stgs.instructions.size() != 6) return 1;
    for (const auto& instruction : stgs.instructions)
    {
        if (instruction.opcode != simtlab::Opcode::STG || instruction.operands.size() != 3
            || !std::holds_alternative<simtlab::MemoryOperand>(instruction.operands[0])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[1])
            || !std::holds_alternative<simtlab::MemoryWidth>(instruction.operands[2])) return 1;
    }
    if (std::get<simtlab::MemoryOperand>(stgs.instructions[0].operands[0]).base != simtlab::GprIndex::R31
        || !std::holds_alternative<std::monostate>(std::get<simtlab::MemoryOperand>(stgs.instructions[0].operands[0]).offset)
        || std::get<simtlab::GprIndex>(stgs.instructions[0].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::MemoryWidth>(stgs.instructions[0].operands[2]) != simtlab::MemoryWidth::BYTE
        || std::get<std::int32_t>(std::get<simtlab::MemoryOperand>(stgs.instructions[1].operands[0]).offset) != 4
        || std::get<simtlab::GprIndex>(stgs.instructions[1].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::MemoryWidth>(stgs.instructions[1].operands[2]) != simtlab::MemoryWidth::HALFWORD
        || std::get<std::int32_t>(std::get<simtlab::MemoryOperand>(stgs.instructions[2].operands[0]).offset) != -16
        || std::get<simtlab::GprIndex>(std::get<simtlab::MemoryOperand>(stgs.instructions[3].operands[0]).offset) != simtlab::GprIndex::R31
        || std::get<std::int32_t>(std::get<simtlab::MemoryOperand>(stgs.instructions[4].operands[0]).offset) != std::int32_t{2147483647}
        || std::get<std::int32_t>(std::get<simtlab::MemoryOperand>(stgs.instructions[5].operands[0]).offset) != std::int32_t{-2147483647 - 1}
        || std::get<simtlab::MemoryWidth>(stgs.instructions[5].operands[2]) != simtlab::MemoryWidth::BYTE) return 1;
    if (!rejects("STG", "1:4") || !rejects("STG r0, [r1], 4", "1:5")
        || !rejects("STG [r32], r0, 4", "1:6") || !rejects("STG [r1+r32], r0, 4", "1:9")
        || !rejects("STG [r1-r2], r0, 4", "1:9") || !rejects("STG [r1+1.5], r0, 4", "1:9")
        || !rejects("STG [r1+2147483648], r0, 4", "1:9") || !rejects("STG [r1-2147483649], r0, 4", "1:9")
        || !rejects("STG [r1, r0, 4", "1:8") || !rejects("STG r1], r0, 4", "1:5")
        || !rejects("STG [r1],", "1:10") || !rejects("STG [r1], r32, 4", "1:11")
        || !rejects("STG [r1] r0, 4", "1:10") || !rejects("STG [r1], r0", "1:13")
        || !rejects("STG [r1], r0 4", "1:14") || !rejects("STG [r1], r0,", "1:14")
        || !rejects("STG [r1], r0, 3", "1:15") || !rejects("STG [r1], r0, 0", "1:15")
        || !rejects("STG [r1], r0, 8", "1:15") || !rejects("STG [r1], r0, 1.5", "1:15")
        || !rejects("STG [r1], r0, 4, 1", "1:16") || !rejects("STG [r1+], r0, 4", "1:9")) return 1;
    const auto ldss = simtlab::parse("LDS r0, [r31], 1\nlds R31, [r0+4], 2\nLdS r1, [r2-0x10], 4"
                                     "\nLDS r2, [r3+r31], 4\nLDS r4, [r5+2147483647], 4"
                                     "\nLDS r6, [r7-0x80000000], 1");
    if (ldss.instructions.size() != 6) return 1;
    for (const auto& instruction : ldss.instructions)
        if (instruction.opcode != simtlab::Opcode::LDS || instruction.operands.size() != 3
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[0])
            || !std::holds_alternative<simtlab::MemoryOperand>(instruction.operands[1])
            || !std::holds_alternative<simtlab::MemoryWidth>(instruction.operands[2])) return 1;
    if (std::get<simtlab::GprIndex>(ldss.instructions[0].operands[0]) != simtlab::GprIndex::R0
        || std::get<simtlab::MemoryOperand>(ldss.instructions[0].operands[1]).base != simtlab::GprIndex::R31
        || !std::holds_alternative<std::monostate>(std::get<simtlab::MemoryOperand>(ldss.instructions[0].operands[1]).offset)
        || std::get<simtlab::MemoryWidth>(ldss.instructions[0].operands[2]) != simtlab::MemoryWidth::BYTE
        || std::get<std::int32_t>(std::get<simtlab::MemoryOperand>(ldss.instructions[1].operands[1]).offset) != 4
        || std::get<simtlab::MemoryWidth>(ldss.instructions[1].operands[2]) != simtlab::MemoryWidth::HALFWORD
        || std::get<std::int32_t>(std::get<simtlab::MemoryOperand>(ldss.instructions[2].operands[1]).offset) != -16
        || std::get<simtlab::GprIndex>(std::get<simtlab::MemoryOperand>(ldss.instructions[3].operands[1]).offset) != simtlab::GprIndex::R31
        || std::get<std::int32_t>(std::get<simtlab::MemoryOperand>(ldss.instructions[4].operands[1]).offset) != std::int32_t{2147483647}
        || std::get<std::int32_t>(std::get<simtlab::MemoryOperand>(ldss.instructions[5].operands[1]).offset) != std::int32_t{-2147483647 - 1}
        || std::get<simtlab::MemoryWidth>(ldss.instructions[2].operands[2]) != simtlab::MemoryWidth::WORD) return 1;
    if (!rejects("LDS", "1:4") || !rejects("LDS r32, [r1], 4", "1:5")
        || !rejects("LDS rx, [r1], 4", "1:5") || !rejects("LDS r0", "1:7")
        || !rejects("LDS r0, r1, 4", "1:9") || !rejects("LDS r0, [r1, 4", "1:12")
        || !rejects("LDS r0, r1], 4", "1:9") || !rejects("LDS r0, [r32], 4", "1:10")
        || !rejects("LDS r0, [r1+r32], 4", "1:13") || !rejects("LDS r0, [r1-r2], 4", "1:13")
        || !rejects("LDS r0, [r1+1.5], 4", "1:13") || !rejects("LDS r0, [r1+2147483648], 4", "1:13")
        || !rejects("LDS r0, [r1-2147483649], 4", "1:13") || !rejects("LDS r0, [r1],", "1:14")
        || !rejects("LDS r0, [r1], 3", "1:15") || !rejects("LDS r0, [r1], 0", "1:15")
        || !rejects("LDS r0, [r1], 8", "1:15") || !rejects("LDS r0, [r1], 1.5", "1:15")
        || !rejects("LDS r0, [r1], 4, 1", "1:16") || !rejects("LDS r0 [r1], 4", "1:8")
        || !rejects("LDS r0, [r1] 4", "1:14") || !rejects("LDS r0, [r1+], 4", "1:13")) return 1;
    const auto stss = simtlab::parse("STS [r31], r0, 1\nsts [r0+4], R31, 2\nStS [r2-0x10], r3, 4"
                                     "\nSTS [r3+r31], r2, 4\nSTS [r5+2147483647], r4, 4"
                                     "\nSTS [r7-0x80000000], r6, 1");
    if (stss.instructions.size() != 6) return 1;
    for (const auto& instruction : stss.instructions)
        if (instruction.opcode != simtlab::Opcode::STS || instruction.operands.size() != 3
            || !std::holds_alternative<simtlab::MemoryOperand>(instruction.operands[0])
            || !std::holds_alternative<simtlab::GprIndex>(instruction.operands[1])
            || !std::holds_alternative<simtlab::MemoryWidth>(instruction.operands[2])) return 1;
    if (std::get<simtlab::MemoryOperand>(stss.instructions[0].operands[0]).base != simtlab::GprIndex::R31
        || !std::holds_alternative<std::monostate>(std::get<simtlab::MemoryOperand>(stss.instructions[0].operands[0]).offset)
        || std::get<simtlab::GprIndex>(stss.instructions[0].operands[1]) != simtlab::GprIndex::R0
        || std::get<simtlab::MemoryWidth>(stss.instructions[0].operands[2]) != simtlab::MemoryWidth::BYTE
        || std::get<std::int32_t>(std::get<simtlab::MemoryOperand>(stss.instructions[1].operands[0]).offset) != 4
        || std::get<simtlab::GprIndex>(stss.instructions[1].operands[1]) != simtlab::GprIndex::R31
        || std::get<simtlab::MemoryWidth>(stss.instructions[1].operands[2]) != simtlab::MemoryWidth::HALFWORD
        || std::get<std::int32_t>(std::get<simtlab::MemoryOperand>(stss.instructions[2].operands[0]).offset) != -16
        || std::get<simtlab::GprIndex>(std::get<simtlab::MemoryOperand>(stss.instructions[3].operands[0]).offset) != simtlab::GprIndex::R31
        || std::get<std::int32_t>(std::get<simtlab::MemoryOperand>(stss.instructions[4].operands[0]).offset) != std::int32_t{2147483647}
        || std::get<std::int32_t>(std::get<simtlab::MemoryOperand>(stss.instructions[5].operands[0]).offset) != std::int32_t{-2147483647 - 1}
        || std::get<simtlab::MemoryWidth>(stss.instructions[2].operands[2]) != simtlab::MemoryWidth::WORD) return 1;
    if (!rejects("STS", "1:4") || !rejects("STS r0, [r1], 4", "1:5")
        || !rejects("STS [r32], r0, 4", "1:6") || !rejects("STS [rx], r0, 4", "1:6")
        || !rejects("STS [r1+r32], r0, 4", "1:9") || !rejects("STS [r1-r2], r0, 4", "1:9")
        || !rejects("STS [r1+1.5], r0, 4", "1:9") || !rejects("STS [r1+2147483648], r0, 4", "1:9")
        || !rejects("STS [r1-2147483649], r0, 4", "1:9") || !rejects("STS [r1, r0, 4", "1:8")
        || !rejects("STS r1], r0, 4", "1:5") || !rejects("STS [r1],", "1:10")
        || !rejects("STS [r1], r32, 4", "1:11") || !rejects("STS [r1] r0, 4", "1:10")
        || !rejects("STS [r1], r0", "1:13") || !rejects("STS [r1], r0 4", "1:14")
        || !rejects("STS [r1], r0,", "1:14") || !rejects("STS [r1], r0, 3", "1:15")
        || !rejects("STS [r1], r0, 0", "1:15") || !rejects("STS [r1], r0, 8", "1:15")
        || !rejects("STS [r1], r0, 1.5", "1:15") || !rejects("STS [r1], r0, 4, 1", "1:16")
        || !rejects("STS [r1+], r0, 4", "1:9")) return 1;
    return 0;
}
