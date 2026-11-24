#pragma once

#include <simtlab/isa/instruction.hpp>

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace simtlab
{

struct Program
{
    std::vector<Instruction> instructions;
    std::map<std::string, std::size_t> labels;
};

Program parse(std::string_view source);

} // namespace simtlab
