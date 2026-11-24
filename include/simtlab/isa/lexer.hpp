#pragma once

#include <simtlab/isa/token.hpp>

#include <string_view>
#include <vector>

namespace simtlab
{

std::vector<Token> lex(std::string_view source);

} // namespace simtlab
