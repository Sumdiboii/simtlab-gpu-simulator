#include <simtlab/isa/operand.hpp>

#include <variant>

int main()
{
    using namespace simtlab;

    const Operand gpr = GprIndex::R10;
    const Operand predicate = PredicateIndex::P2;
    const Operand signed_immediate = std::int32_t{-4};
    const Operand unsigned_immediate = std::uint32_t{0xff};
    const Operand floating_immediate = 1.25f;
    const Operand memory = MemoryOperand{GprIndex::R10, std::int32_t{-4}};
    const Operand base_memory = MemoryOperand{GprIndex::R11, std::monostate{}};
    const Operand register_memory = MemoryOperand{GprIndex::R12, GprIndex::R1};
    const Operand label = LabelOperand{"done"};
    const Operand width = MemoryWidth::WORD;
    const Operand special = SpecialRegister::TID_X;

    if (!std::holds_alternative<GprIndex>(gpr)
        || !std::holds_alternative<PredicateIndex>(predicate)
        || !std::holds_alternative<std::int32_t>(signed_immediate)
        || !std::holds_alternative<std::uint32_t>(unsigned_immediate)
        || !std::holds_alternative<float>(floating_immediate)
        || !std::holds_alternative<MemoryOperand>(memory)
        || !std::holds_alternative<MemoryOperand>(base_memory)
        || !std::holds_alternative<MemoryOperand>(register_memory)
        || !std::holds_alternative<LabelOperand>(label)
        || !std::holds_alternative<MemoryWidth>(width)
        || !std::holds_alternative<SpecialRegister>(special))
    {
        return 1;
    }

    const auto& memory_operand = std::get<MemoryOperand>(memory);
    if (memory_operand.base != GprIndex::R10
        || !std::holds_alternative<std::int32_t>(memory_operand.offset)
        || std::get<std::int32_t>(memory_operand.offset) != -4)
    {
        return 1;
    }

    return 0;
}
