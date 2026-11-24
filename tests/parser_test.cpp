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
    return 0;
}
