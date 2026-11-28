#pragma once

#include <simtlab/isa/opcode.hpp>
#include <simtlab/isa/operand.hpp>
#include "simtlab/isa/register.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace simtlab
{

struct Predicate
{
    PredicateIndex index;
    bool negated;
};

struct SourceLocation
{
    std::size_t line;
    std::size_t column;
};

struct Instruction
{
    Opcode opcode;
    std::vector<Operand> operands;
    std::optional<Predicate> predicate;
    // Empty for programmatically constructed instructions; runtime diagnostics then say location unavailable.
    std::optional<SourceLocation> source_location;
};

} // namespace simtlab
