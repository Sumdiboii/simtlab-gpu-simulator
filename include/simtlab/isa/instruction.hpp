#pragma once

#include <simtlab/isa/opcode.hpp>
#include <simtlab/isa/operand.hpp>
#include "simtlab/isa/register.hpp"

#include <optional>
#include <vector>

namespace simtlab
{

struct Predicate
{
    PredicateIndex index;
    bool negated;
};

struct Instruction
{
    Opcode opcode;
    std::vector<Operand> operands;
    std::optional<Predicate> predicate;
};

} // namespace simtlab
