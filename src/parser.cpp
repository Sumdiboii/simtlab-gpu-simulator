#include <simtlab/isa/lexer.hpp>
#include <simtlab/isa/parser.hpp>
#include <simtlab/isa/register.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace simtlab
{
namespace
{
bool matchesMnemonic(std::string_view name, std::string_view expected)
{
    if (name.size() != expected.size()) return false;
    for (std::size_t i = 0; i < name.size(); ++i)
    {
        const char upper = name[i] >= 'a' && name[i] <= 'z' ? name[i] - 'a' + 'A' : name[i];
        if (upper != expected[i]) return false;
    }
    return true;
}

bool parseGpr(std::string_view name, GprIndex& index)
{
    if (name.size() < 2 || (name[0] != 'r' && name[0] != 'R')
        || (name.size() > 2 && name[1] == '0')) return false;
    unsigned int value = 0;
    for (std::size_t i = 1; i < name.size(); ++i)
    {
        if (name[i] < '0' || name[i] > '9') return false;
        value = value * 10 + static_cast<unsigned int>(name[i] - '0');
        if (value > MAX_GPR_INDEX) return false;
    }
    index = static_cast<GprIndex>(value);
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
        if (tokens[position].kind == TokenKind::Colon)
        {
            if (program.labels.find(mnemonic.lexeme) != program.labels.end())
                fail(mnemonic, "duplicate label");
            if (tokens[position + 1].kind != TokenKind::EndOfLine
                && tokens[position + 1].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position + 1], "expected end of label line");
            }
            program.labels.emplace(mnemonic.lexeme, program.instructions.size());
            ++position;
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        const bool isBarrier = matchesMnemonic(mnemonic.lexeme, "BAR")
            && tokens[position].kind == TokenKind::Dot
            && position + 1 < tokens.size()
            && tokens[position + 1].kind == TokenKind::Identifier
            && matchesMnemonic(tokens[position + 1].lexeme, "SYNC");
        const bool isAdd = matchesMnemonic(mnemonic.lexeme, "ADD");
        const bool isSub = matchesMnemonic(mnemonic.lexeme, "SUB");
        const bool isMul = matchesMnemonic(mnemonic.lexeme, "MUL");
        if (isBarrier) position += 2;
        else if (!matchesMnemonic(mnemonic.lexeme, "EXIT") && !isAdd && !isSub && !isMul) fail(mnemonic, "unknown mnemonic");
        if (isBarrier)
        {
            const Token* sign = nullptr;
            bool negative = false;
            if (tokens[position].kind == TokenKind::Plus || tokens[position].kind == TokenKind::Minus)
            {
                sign = &tokens[position++];
                negative = sign->kind == TokenKind::Minus;
            }
            const Token& literal = tokens[position];
            if (literal.kind != TokenKind::IntegerLiteral) fail(literal, "expected integer barrier ID");
            ++position;
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after BAR.SYNC");
            }
            std::string_view digits = literal.lexeme;
            unsigned int base = 10;
            if (digits.size() > 2 && digits[0] == '0' && digits[1] == 'x')
            {
                digits.remove_prefix(2);
                base = 16;
            }
            unsigned int value = 0;
            for (const char digit_char : digits)
            {
                const unsigned int digit = digit_char >= '0' && digit_char <= '9'
                    ? static_cast<unsigned int>(digit_char - '0')
                    : static_cast<unsigned int>((digit_char | 0x20) - 'a' + 10);
                value = value * base + digit;
                if (value > 15) fail(sign ? *sign : literal, "BAR.SYNC ID must be 0 through 15");
            }
            if (negative && value != 0) fail(*sign, "BAR.SYNC ID must be 0 through 15");
            program.instructions.push_back({Opcode::BAR_SYNC, {std::uint32_t{value}}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isAdd)
        {
            auto registerOperand = [&]()
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, "expected GPR register r0-r31");
                ++position;
                return index;
            };
            const GprIndex destination = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in ADD");
            ++position;
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in ADD");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after ADD");
            }
            program.instructions.push_back({Opcode::ADD, {destination, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isSub)
        {
            auto registerOperand = [&]()
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, "expected GPR register r0-r31");
                ++position;
                return index;
            };
            const GprIndex destination = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SUB");
            ++position;
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SUB");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after SUB");
            }
            program.instructions.push_back({Opcode::SUB, {destination, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isMul)
        {
            auto registerOperand = [&]()
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, "expected GPR register r0-r31");
                ++position;
                return index;
            };
            const GprIndex destination = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in MUL");
            ++position;
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in MUL");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after MUL");
            }
            program.instructions.push_back({Opcode::MUL, {destination, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
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
