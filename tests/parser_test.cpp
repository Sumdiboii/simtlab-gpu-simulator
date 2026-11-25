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
    if (!rejects("NOPE", "1:1") || !rejects("EXIT r0", "1:6") || !rejects("@EXIT", "1:1")
        || !rejects("\n  nope", "2:3")) return 1;
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
        || !rejects("ADD rx, r1, r2", "1:5") || !rejects("ADD r32, r1, r2", "1:5")
        || !rejects("ADD r0, r1, 2", "1:13")) return 1;
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
        || !rejects("SUB rx, r1, r2", "1:5") || !rejects("SUB r32, r1, r2", "1:5")
        || !rejects("SUB r0, r1, 2", "1:13")) return 1;
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
    return 0;
}
