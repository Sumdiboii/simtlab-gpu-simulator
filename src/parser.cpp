#include <simtlab/isa/lexer.hpp>
#include <simtlab/isa/parser.hpp>

#include <stdexcept>
#include <string>

namespace simtlab
{
namespace
{
bool isExit(std::string_view name)
{
    constexpr std::string_view expected = "EXIT";
    if (name.size() != expected.size()) return false;
    for (std::size_t i = 0; i < name.size(); ++i)
    {
        const char upper = name[i] >= 'a' && name[i] <= 'z' ? name[i] - 'a' + 'A' : name[i];
        if (upper != expected[i]) return false;
    }
    return true;
}
}
Program parse(std::string_view source)
{
    const auto tokens = lex(source);
    Program program;
    std::size_t position = 0;
    auto fail = [](const Token& token, const char* message)
    {
        throw std::runtime_error(std::string(message) + " at "
                                 + std::to_string(token.line) + ":" + std::to_string(token.column));
    };
    while (tokens[position].kind != TokenKind::EndOfFile)
    {
        if (tokens[position].kind == TokenKind::EndOfLine)
        {
            ++position;
            continue;
        }
        const Token& mnemonic = tokens[position++];
        if (mnemonic.kind != TokenKind::Identifier) fail(mnemonic, "malformed instruction");
        if (!isExit(mnemonic.lexeme)) fail(mnemonic, "unknown mnemonic");
        if (tokens[position].kind != TokenKind::EndOfLine
            && tokens[position].kind != TokenKind::EndOfFile)
        {
            fail(tokens[position], "unexpected operand after EXIT");
        }
        program.instructions.push_back({Opcode::EXIT, {}, {}});
        if (tokens[position].kind == TokenKind::EndOfLine) ++position;
    }
    return program;
}

} // namespace simtlab
