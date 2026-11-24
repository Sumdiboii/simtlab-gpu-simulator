#pragma once

#include <cstddef>
#include <string>

namespace simtlab
{

enum class TokenKind
{
    Identifier,
    IntegerLiteral,
    FloatLiteral,
    AtSign,
    Bang,
    Comma,
    Colon,
    Dot,
    LeftBracket,
    RightBracket,
    Plus,
    Minus,
    EndOfLine,
    EndOfFile
};

struct Token
{
    TokenKind kind;
    std::string lexeme;
    std::size_t line;
    std::size_t column;
};

} // namespace simtlab
