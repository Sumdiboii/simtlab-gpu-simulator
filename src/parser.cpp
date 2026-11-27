#include <simtlab/isa/lexer.hpp>
#include <simtlab/isa/parser.hpp>
#include <simtlab/isa/register.hpp>

#include <cstdint>
#include <limits>
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
        const bool isFma = matchesMnemonic(mnemonic.lexeme, "FMA");
        const bool isLdg = matchesMnemonic(mnemonic.lexeme, "LDG");
        const bool isStg = matchesMnemonic(mnemonic.lexeme, "STG");
        const bool isLds = matchesMnemonic(mnemonic.lexeme, "LDS");
        const bool isSts = matchesMnemonic(mnemonic.lexeme, "STS");
        const bool isBra = matchesMnemonic(mnemonic.lexeme, "BRA");
        const bool isJmp = matchesMnemonic(mnemonic.lexeme, "JMP");
        const bool isAnd = matchesMnemonic(mnemonic.lexeme, "AND");
        const bool isOr = matchesMnemonic(mnemonic.lexeme, "OR");
        const bool isShl = matchesMnemonic(mnemonic.lexeme, "SHL");
        const bool isShr = matchesMnemonic(mnemonic.lexeme, "SHR");
        const bool isFadd = matchesMnemonic(mnemonic.lexeme, "FADD");
        const bool isFmul = matchesMnemonic(mnemonic.lexeme, "FMUL");
        const bool isSetpEq = matchesMnemonic(mnemonic.lexeme, "SETP")
            && tokens[position].kind == TokenKind::Dot
            && position + 1 < tokens.size()
            && tokens[position + 1].kind == TokenKind::Identifier
            && matchesMnemonic(tokens[position + 1].lexeme, "EQ");
        const bool isSetpNe = matchesMnemonic(mnemonic.lexeme, "SETP")
            && tokens[position].kind == TokenKind::Dot
            && position + 1 < tokens.size()
            && tokens[position + 1].kind == TokenKind::Identifier
            && matchesMnemonic(tokens[position + 1].lexeme, "NE");
        const bool isSetpLt = matchesMnemonic(mnemonic.lexeme, "SETP")
            && tokens[position].kind == TokenKind::Dot
            && position + 1 < tokens.size()
            && tokens[position + 1].kind == TokenKind::Identifier
            && matchesMnemonic(tokens[position + 1].lexeme, "LT");
        const bool isSetpLe = matchesMnemonic(mnemonic.lexeme, "SETP")
            && tokens[position].kind == TokenKind::Dot
            && position + 1 < tokens.size()
            && tokens[position + 1].kind == TokenKind::Identifier
            && matchesMnemonic(tokens[position + 1].lexeme, "LE");
        const bool isSetpGt = matchesMnemonic(mnemonic.lexeme, "SETP")
            && tokens[position].kind == TokenKind::Dot
            && position + 1 < tokens.size()
            && tokens[position + 1].kind == TokenKind::Identifier
            && matchesMnemonic(tokens[position + 1].lexeme, "GT");
        const bool isSetpGe = matchesMnemonic(mnemonic.lexeme, "SETP")
            && tokens[position].kind == TokenKind::Dot
            && position + 1 < tokens.size()
            && tokens[position + 1].kind == TokenKind::Identifier
            && matchesMnemonic(tokens[position + 1].lexeme, "GE");
        const bool isSetpFeq = matchesMnemonic(mnemonic.lexeme, "SETP")
            && tokens[position].kind == TokenKind::Dot
            && position + 1 < tokens.size()
            && tokens[position + 1].kind == TokenKind::Identifier
            && matchesMnemonic(tokens[position + 1].lexeme, "FEQ");
        const bool isSetpFne = matchesMnemonic(mnemonic.lexeme, "SETP")
            && tokens[position].kind == TokenKind::Dot
            && position + 1 < tokens.size()
            && tokens[position + 1].kind == TokenKind::Identifier
            && matchesMnemonic(tokens[position + 1].lexeme, "FNE");
        const bool isSetpFlt = matchesMnemonic(mnemonic.lexeme, "SETP")
            && tokens[position].kind == TokenKind::Dot
            && position + 1 < tokens.size()
            && tokens[position + 1].kind == TokenKind::Identifier
            && matchesMnemonic(tokens[position + 1].lexeme, "FLT");
        const bool isSetpFle = matchesMnemonic(mnemonic.lexeme, "SETP")
            && tokens[position].kind == TokenKind::Dot
            && position + 1 < tokens.size()
            && tokens[position + 1].kind == TokenKind::Identifier
            && matchesMnemonic(tokens[position + 1].lexeme, "FLE");
        const bool isSetpFgt = matchesMnemonic(mnemonic.lexeme, "SETP")
            && tokens[position].kind == TokenKind::Dot
            && position + 1 < tokens.size()
            && tokens[position + 1].kind == TokenKind::Identifier
            && matchesMnemonic(tokens[position + 1].lexeme, "FGT");
        const bool isSetpFge = matchesMnemonic(mnemonic.lexeme, "SETP")
            && tokens[position].kind == TokenKind::Dot
            && position + 1 < tokens.size()
            && tokens[position + 1].kind == TokenKind::Identifier
            && matchesMnemonic(tokens[position + 1].lexeme, "FGE");
        if (isBarrier) position += 2;
        else if (!matchesMnemonic(mnemonic.lexeme, "EXIT") && !isAdd && !isSub && !isMul && !isFma && !isLdg && !isStg && !isLds && !isSts && !isBra && !isJmp && !isAnd && !isOr && !isShl && !isShr && !isFadd && !isFmul && !isSetpEq && !isSetpNe && !isSetpLt && !isSetpLe && !isSetpGt && !isSetpGe && !isSetpFeq && !isSetpFne && !isSetpFlt && !isSetpFle && !isSetpFgt && !isSetpFge) fail(mnemonic, "unknown mnemonic");
        if (isSetpEq) position += 2;
        if (isSetpNe) position += 2;
        if (isSetpLt) position += 2;
        if (isSetpLe) position += 2;
        if (isSetpGt) position += 2;
        if (isSetpGe) position += 2;
        if (isSetpFeq) position += 2;
        if (isSetpFne) position += 2;
        if (isSetpFlt) position += 2;
        if (isSetpFle) position += 2;
        if (isSetpFgt) position += 2;
        if (isSetpFge) position += 2;
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
        if (isBra)
        {
            const Token& target = tokens[position];
            if (target.kind != TokenKind::Identifier) fail(target, "expected BRA target label");
            ++position;
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma after BRA target");
            ++position;
            const Token& reconv = tokens[position];
            if (reconv.kind != TokenKind::Identifier) fail(reconv, "expected BRA reconvergence label");
            ++position;
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
                fail(tokens[position], "unexpected operand after BRA");
            program.instructions.push_back({Opcode::BRA,
                {LabelOperand{target.lexeme}, LabelOperand{reconv.lexeme}}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isJmp)
        {
            const Token& target = tokens[position];
            if (target.kind != TokenKind::Identifier) fail(target, "expected JMP target label");
            ++position;
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
                fail(tokens[position], "unexpected operand after JMP");
            program.instructions.push_back({Opcode::JMP, {LabelOperand{target.lexeme}}, {}});
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
        if (isFma)
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
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in FMA");
            ++position;
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in FMA");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in FMA");
            ++position;
            const GprIndex source3 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after FMA");
            }
            program.instructions.push_back({Opcode::FMA, {destination, source1, source2, source3}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isAnd)
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
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in AND");
            ++position;
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in AND");
            ++position;
            Operand source2;
            if (tokens[position].kind == TokenKind::Identifier)
            {
                source2 = registerOperand();
            }
            else
            {
                bool negative = false;
                if (tokens[position].kind == TokenKind::Plus || tokens[position].kind == TokenKind::Minus)
                {
                    negative = tokens[position].kind == TokenKind::Minus;
                    ++position;
                }
                const Token& literal = tokens[position];
                if (literal.kind != TokenKind::IntegerLiteral) fail(literal, "expected GPR or integer immediate in AND");
                ++position;
                std::string_view digits = literal.lexeme;
                unsigned int base = 10;
                if (digits.size() > 2 && digits[0] == '0' && digits[1] == 'x')
                {
                    digits.remove_prefix(2);
                    base = 16;
                }
                std::uint32_t value = 0;
                for (const char character : digits)
                {
                    unsigned int digit = 0;
                    if (character >= '0' && character <= '9')
                        digit = static_cast<unsigned int>(character - '0');
                    else if (base == 16 && character >= 'a' && character <= 'f')
                        digit = static_cast<unsigned int>(character - 'a' + 10);
                    else if (base == 16 && character >= 'A' && character <= 'F')
                        digit = static_cast<unsigned int>(character - 'A' + 10);
                    else
                        fail(literal, "invalid digit in AND immediate");
                    if (value > (std::numeric_limits<std::uint32_t>::max() - digit) / base)
                        fail(literal, "AND immediate out of range");
                    value = value * base + digit;
                }
                source2 = negative ? std::uint32_t{0} - value : value;
            }
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after AND");
            }
            program.instructions.push_back({Opcode::AND, {destination, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isOr)
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
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in OR");
            ++position;
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in OR");
            ++position;
            Operand source2;
            if (tokens[position].kind == TokenKind::Identifier)
            {
                source2 = registerOperand();
            }
            else
            {
                bool negative = false;
                if (tokens[position].kind == TokenKind::Plus || tokens[position].kind == TokenKind::Minus)
                {
                    negative = tokens[position].kind == TokenKind::Minus;
                    ++position;
                }
                const Token& literal = tokens[position];
                if (literal.kind != TokenKind::IntegerLiteral) fail(literal, "expected GPR or integer immediate in OR");
                ++position;
                std::string_view digits = literal.lexeme;
                unsigned int base = 10;
                if (digits.size() > 2 && digits[0] == '0' && digits[1] == 'x')
                {
                    digits.remove_prefix(2);
                    base = 16;
                }
                std::uint32_t value = 0;
                for (const char character : digits)
                {
                    unsigned int digit = 0;
                    if (character >= '0' && character <= '9')
                        digit = static_cast<unsigned int>(character - '0');
                    else if (base == 16 && character >= 'a' && character <= 'f')
                        digit = static_cast<unsigned int>(character - 'a' + 10);
                    else if (base == 16 && character >= 'A' && character <= 'F')
                        digit = static_cast<unsigned int>(character - 'A' + 10);
                    else
                        fail(literal, "invalid digit in OR immediate");
                    if (value > (std::numeric_limits<std::uint32_t>::max() - digit) / base)
                        fail(literal, "OR immediate out of range");
                    value = value * base + digit;
                }
                source2 = negative ? std::uint32_t{0} - value : value;
            }
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after OR");
            }
            program.instructions.push_back({Opcode::OR, {destination, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isShl)
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
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SHL");
            ++position;
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SHL");
            ++position;
            Operand source2;
            if (tokens[position].kind == TokenKind::Identifier)
            {
                source2 = registerOperand();
            }
            else
            {
                bool negative = false;
                if (tokens[position].kind == TokenKind::Plus || tokens[position].kind == TokenKind::Minus)
                {
                    negative = tokens[position].kind == TokenKind::Minus;
                    ++position;
                }
                const Token& literal = tokens[position];
                if (literal.kind != TokenKind::IntegerLiteral) fail(literal, "expected GPR or integer immediate in SHL");
                ++position;
                std::string_view digits = literal.lexeme;
                unsigned int base = 10;
                if (digits.size() > 2 && digits[0] == '0' && digits[1] == 'x')
                {
                    digits.remove_prefix(2);
                    base = 16;
                }
                std::uint32_t value = 0;
                for (const char character : digits)
                {
                    unsigned int digit = 0;
                    if (character >= '0' && character <= '9')
                        digit = static_cast<unsigned int>(character - '0');
                    else if (base == 16 && character >= 'a' && character <= 'f')
                        digit = static_cast<unsigned int>(character - 'a' + 10);
                    else if (base == 16 && character >= 'A' && character <= 'F')
                        digit = static_cast<unsigned int>(character - 'A' + 10);
                    else
                        fail(literal, "invalid digit in SHL immediate");
                    if (value > (std::numeric_limits<std::uint32_t>::max() - digit) / base)
                        fail(literal, "SHL immediate out of range");
                    value = value * base + digit;
                }
                source2 = negative ? std::uint32_t{0} - value : value;
            }
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after SHL");
            }
            program.instructions.push_back({Opcode::SHL, {destination, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isShr)
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
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SHR");
            ++position;
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SHR");
            ++position;
            Operand source2;
            if (tokens[position].kind == TokenKind::Identifier)
            {
                source2 = registerOperand();
            }
            else
            {
                bool negative = false;
                if (tokens[position].kind == TokenKind::Plus || tokens[position].kind == TokenKind::Minus)
                {
                    negative = tokens[position].kind == TokenKind::Minus;
                    ++position;
                }
                const Token& literal = tokens[position];
                if (literal.kind != TokenKind::IntegerLiteral) fail(literal, "expected GPR or integer immediate in SHR");
                ++position;
                std::string_view digits = literal.lexeme;
                unsigned int base = 10;
                if (digits.size() > 2 && digits[0] == '0' && digits[1] == 'x')
                {
                    digits.remove_prefix(2);
                    base = 16;
                }
                std::uint32_t value = 0;
                for (const char character : digits)
                {
                    unsigned int digit = 0;
                    if (character >= '0' && character <= '9')
                        digit = static_cast<unsigned int>(character - '0');
                    else if (base == 16 && character >= 'a' && character <= 'f')
                        digit = static_cast<unsigned int>(character - 'a' + 10);
                    else if (base == 16 && character >= 'A' && character <= 'F')
                        digit = static_cast<unsigned int>(character - 'A' + 10);
                    else
                        fail(literal, "invalid digit in SHR immediate");
                    if (value > (std::numeric_limits<std::uint32_t>::max() - digit) / base)
                        fail(literal, "SHR immediate out of range");
                    value = value * base + digit;
                }
                source2 = negative ? std::uint32_t{0} - value : value;
            }
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after SHR");
            }
            program.instructions.push_back({Opcode::SHR, {destination, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isFadd)
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
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in FADD");
            ++position;
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in FADD");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after FADD");
            }
            program.instructions.push_back({Opcode::FADD, {destination, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isFmul)
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
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in FMUL");
            ++position;
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in FMUL");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after FMUL");
            }
            program.instructions.push_back({Opcode::FMUL, {destination, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isLdg)
        {
            auto registerOperand = [&](const char* expected)
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, expected);
                ++position;
                return index;
            };
            auto integerMagnitude = [&](const Token& token)
            {
                if (token.kind != TokenKind::IntegerLiteral) fail(token, "expected integer literal in LDG");
                std::string_view digits = token.lexeme;
                unsigned int base = 10;
                if (digits.size() > 2 && digits[0] == '0' && digits[1] == 'x')
                {
                    digits.remove_prefix(2);
                    base = 16;
                }
                std::uint64_t value = 0;
                for (const char character : digits)
                {
                    unsigned int digit = 0;
                    if (character >= '0' && character <= '9')
                        digit = static_cast<unsigned int>(character - '0');
                    else if (base == 16 && character >= 'a' && character <= 'f')
                        digit = static_cast<unsigned int>(character - 'a' + 10);
                    else if (base == 16 && character >= 'A' && character <= 'F')
                        digit = static_cast<unsigned int>(character - 'A' + 10);
                    else
                        fail(token, "invalid digit in LDG integer literal");
                    if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / base)
                        fail(token, "LDG integer literal out of range");
                    value = value * base + digit;
                }
                return value;
            };

            const GprIndex destination = registerOperand("expected GPR destination r0-r31 in LDG");
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma after LDG destination");
            ++position;
            if (tokens[position].kind != TokenKind::LeftBracket) fail(tokens[position], "expected '[' before LDG address");
            ++position;
            const GprIndex base = registerOperand("expected GPR base register r0-r31 in LDG address");
            std::variant<std::monostate, std::int32_t, GprIndex> offset;
            if (tokens[position].kind == TokenKind::Plus)
            {
                ++position;
                if (tokens[position].kind == TokenKind::Identifier)
                {
                    offset = registerOperand("expected GPR offset register r0-r31");
                }
                else
                {
                    const Token& literal = tokens[position++];
                    const std::uint64_t value = integerMagnitude(literal);
                    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
                        fail(literal, "LDG displacement out of signed 32-bit range");
                    offset = static_cast<std::int32_t>(value);
                }
            }
            else if (tokens[position].kind == TokenKind::Minus)
            {
                ++position;
                const Token& literal = tokens[position++];
                const std::uint64_t value = integerMagnitude(literal);
                const std::uint64_t minMagnitude = std::uint64_t{1} << 31;
                if (value > minMagnitude) fail(literal, "LDG displacement out of signed 32-bit range");
                offset = value == minMagnitude ? std::numeric_limits<std::int32_t>::min()
                                               : -static_cast<std::int32_t>(value);
            }
            if (tokens[position].kind != TokenKind::RightBracket) fail(tokens[position], "expected ']' after LDG address");
            ++position;
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma after LDG address");
            ++position;
            const Token& widthToken = tokens[position++];
            const std::uint64_t width = integerMagnitude(widthToken);
            if (width != 1 && width != 2 && width != 4) fail(widthToken, "LDG width must be 1, 2, or 4");
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after LDG");
            }
            program.instructions.push_back({Opcode::LDG,
                {destination, MemoryOperand{base, offset}, static_cast<MemoryWidth>(width)}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isStg)
        {
            auto registerOperand = [&](const char* expected)
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, expected);
                ++position;
                return index;
            };
            auto integerMagnitude = [&](const Token& token)
            {
                if (token.kind != TokenKind::IntegerLiteral) fail(token, "expected integer literal in STG");
                std::string_view digits = token.lexeme;
                unsigned int base = 10;
                if (digits.size() > 2 && digits[0] == '0' && digits[1] == 'x')
                {
                    digits.remove_prefix(2);
                    base = 16;
                }
                std::uint64_t value = 0;
                for (const char character : digits)
                {
                    unsigned int digit = 0;
                    if (character >= '0' && character <= '9')
                        digit = static_cast<unsigned int>(character - '0');
                    else if (base == 16 && character >= 'a' && character <= 'f')
                        digit = static_cast<unsigned int>(character - 'a' + 10);
                    else if (base == 16 && character >= 'A' && character <= 'F')
                        digit = static_cast<unsigned int>(character - 'A' + 10);
                    else
                        fail(token, "invalid digit in STG integer literal");
                    if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / base)
                        fail(token, "STG integer literal out of range");
                    value = value * base + digit;
                }
                return value;
            };

            if (tokens[position].kind != TokenKind::LeftBracket) fail(tokens[position], "expected '[' before STG address");
            ++position;
            const GprIndex base = registerOperand("expected GPR base register r0-r31 in STG address");
            std::variant<std::monostate, std::int32_t, GprIndex> offset;
            if (tokens[position].kind == TokenKind::Plus)
            {
                ++position;
                if (tokens[position].kind == TokenKind::Identifier)
                {
                    offset = registerOperand("expected GPR offset register r0-r31");
                }
                else
                {
                    const Token& literal = tokens[position++];
                    const std::uint64_t value = integerMagnitude(literal);
                    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
                        fail(literal, "STG displacement out of signed 32-bit range");
                    offset = static_cast<std::int32_t>(value);
                }
            }
            else if (tokens[position].kind == TokenKind::Minus)
            {
                ++position;
                const Token& literal = tokens[position++];
                const std::uint64_t value = integerMagnitude(literal);
                const std::uint64_t minMagnitude = std::uint64_t{1} << 31;
                if (value > minMagnitude) fail(literal, "STG displacement out of signed 32-bit range");
                offset = value == minMagnitude ? std::numeric_limits<std::int32_t>::min()
                                               : -static_cast<std::int32_t>(value);
            }
            if (tokens[position].kind != TokenKind::RightBracket) fail(tokens[position], "expected ']' after STG address");
            ++position;
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma after STG address");
            ++position;
            const GprIndex source = registerOperand("expected GPR source register r0-r31 in STG");
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma after STG source");
            ++position;
            const Token& widthToken = tokens[position++];
            const std::uint64_t width = integerMagnitude(widthToken);
            if (width != 1 && width != 2 && width != 4) fail(widthToken, "STG width must be 1, 2, or 4");
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after STG");
            }
            program.instructions.push_back({Opcode::STG,
                {MemoryOperand{base, offset}, source, static_cast<MemoryWidth>(width)}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isLds)
        {
            auto registerOperand = [&](const char* expected)
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, expected);
                ++position;
                return index;
            };
            auto integerMagnitude = [&](const Token& token)
            {
                if (token.kind != TokenKind::IntegerLiteral) fail(token, "expected integer literal in LDS");
                std::string_view digits = token.lexeme;
                unsigned int base = 10;
                if (digits.size() > 2 && digits[0] == '0' && digits[1] == 'x')
                {
                    digits.remove_prefix(2);
                    base = 16;
                }
                std::uint64_t value = 0;
                for (const char character : digits)
                {
                    unsigned int digit = 0;
                    if (character >= '0' && character <= '9') digit = static_cast<unsigned int>(character - '0');
                    else if (base == 16 && character >= 'a' && character <= 'f') digit = static_cast<unsigned int>(character - 'a' + 10);
                    else if (base == 16 && character >= 'A' && character <= 'F') digit = static_cast<unsigned int>(character - 'A' + 10);
                    else fail(token, "invalid digit in LDS integer literal");
                    if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / base)
                        fail(token, "LDS integer literal out of range");
                    value = value * base + digit;
                }
                return value;
            };

            const GprIndex destination = registerOperand("expected GPR destination r0-r31 in LDS");
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma after LDS destination");
            ++position;
            if (tokens[position].kind != TokenKind::LeftBracket) fail(tokens[position], "expected '[' before LDS address");
            ++position;
            const GprIndex base = registerOperand("expected GPR base register r0-r31 in LDS address");
            std::variant<std::monostate, std::int32_t, GprIndex> offset;
            if (tokens[position].kind == TokenKind::Plus)
            {
                ++position;
                if (tokens[position].kind == TokenKind::Identifier)
                    offset = registerOperand("expected GPR offset register r0-r31");
                else
                {
                    const Token& literal = tokens[position++];
                    const std::uint64_t value = integerMagnitude(literal);
                    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
                        fail(literal, "LDS displacement out of signed 32-bit range");
                    offset = static_cast<std::int32_t>(value);
                }
            }
            else if (tokens[position].kind == TokenKind::Minus)
            {
                ++position;
                const Token& literal = tokens[position++];
                const std::uint64_t value = integerMagnitude(literal);
                const std::uint64_t minMagnitude = std::uint64_t{1} << 31;
                if (value > minMagnitude) fail(literal, "LDS displacement out of signed 32-bit range");
                offset = value == minMagnitude ? std::numeric_limits<std::int32_t>::min()
                                               : -static_cast<std::int32_t>(value);
            }
            if (tokens[position].kind != TokenKind::RightBracket) fail(tokens[position], "expected ']' after LDS address");
            ++position;
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma after LDS address");
            ++position;
            const Token& widthToken = tokens[position++];
            const std::uint64_t width = integerMagnitude(widthToken);
            if (width != 1 && width != 2 && width != 4) fail(widthToken, "LDS width must be 1, 2, or 4");
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
                fail(tokens[position], "unexpected operand after LDS");
            program.instructions.push_back({Opcode::LDS,
                {destination, MemoryOperand{base, offset}, static_cast<MemoryWidth>(width)}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isSts)
        {
            auto registerOperand = [&](const char* expected)
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, expected);
                ++position;
                return index;
            };
            auto integerMagnitude = [&](const Token& token)
            {
                if (token.kind != TokenKind::IntegerLiteral) fail(token, "expected integer literal in STS");
                std::string_view digits = token.lexeme;
                unsigned int base = 10;
                if (digits.size() > 2 && digits[0] == '0' && digits[1] == 'x')
                {
                    digits.remove_prefix(2);
                    base = 16;
                }
                std::uint64_t value = 0;
                for (const char character : digits)
                {
                    unsigned int digit = 0;
                    if (character >= '0' && character <= '9') digit = static_cast<unsigned int>(character - '0');
                    else if (base == 16 && character >= 'a' && character <= 'f') digit = static_cast<unsigned int>(character - 'a' + 10);
                    else if (base == 16 && character >= 'A' && character <= 'F') digit = static_cast<unsigned int>(character - 'A' + 10);
                    else fail(token, "invalid digit in STS integer literal");
                    if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / base)
                        fail(token, "STS integer literal out of range");
                    value = value * base + digit;
                }
                return value;
            };

            if (tokens[position].kind != TokenKind::LeftBracket) fail(tokens[position], "expected '[' before STS address");
            ++position;
            const GprIndex base = registerOperand("expected GPR base register r0-r31 in STS address");
            std::variant<std::monostate, std::int32_t, GprIndex> offset;
            if (tokens[position].kind == TokenKind::Plus)
            {
                ++position;
                if (tokens[position].kind == TokenKind::Identifier)
                    offset = registerOperand("expected GPR offset register r0-r31");
                else
                {
                    const Token& literal = tokens[position++];
                    const std::uint64_t value = integerMagnitude(literal);
                    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
                        fail(literal, "STS displacement out of signed 32-bit range");
                    offset = static_cast<std::int32_t>(value);
                }
            }
            else if (tokens[position].kind == TokenKind::Minus)
            {
                ++position;
                const Token& literal = tokens[position++];
                const std::uint64_t value = integerMagnitude(literal);
                const std::uint64_t minMagnitude = std::uint64_t{1} << 31;
                if (value > minMagnitude) fail(literal, "STS displacement out of signed 32-bit range");
                offset = value == minMagnitude ? std::numeric_limits<std::int32_t>::min()
                                               : -static_cast<std::int32_t>(value);
            }
            if (tokens[position].kind != TokenKind::RightBracket) fail(tokens[position], "expected ']' after STS address");
            ++position;
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma after STS address");
            ++position;
            const GprIndex source = registerOperand("expected GPR source register r0-r31 in STS");
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma after STS source");
            ++position;
            const Token& widthToken = tokens[position++];
            const std::uint64_t width = integerMagnitude(widthToken);
            if (width != 1 && width != 2 && width != 4) fail(widthToken, "STS width must be 1, 2, or 4");
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
                fail(tokens[position], "unexpected operand after STS");
            program.instructions.push_back({Opcode::STS,
                {MemoryOperand{base, offset}, source, static_cast<MemoryWidth>(width)}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isSetpEq)
        {
            const Token& predicateToken = tokens[position];
            PredicateIndex predicate{};
            if (predicateToken.kind != TokenKind::Identifier || predicateToken.lexeme.size() != 2
                || (predicateToken.lexeme[0] != 'p' && predicateToken.lexeme[0] != 'P')
                || predicateToken.lexeme[1] < '0' || predicateToken.lexeme[1] > '3')
            {
                fail(predicateToken, "expected predicate register p0-p3");
            }
            predicate = static_cast<PredicateIndex>(predicateToken.lexeme[1] - '0');
            ++position;
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.EQ");
            ++position;
            auto registerOperand = [&]()
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, "expected GPR register r0-r31");
                ++position;
                return index;
            };
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.EQ");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after SETP.EQ");
            }
            program.instructions.push_back({Opcode::SETP_EQ, {predicate, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isSetpNe)
        {
            const Token& predicateToken = tokens[position];
            PredicateIndex predicate{};
            if (predicateToken.kind != TokenKind::Identifier || predicateToken.lexeme.size() != 2
                || (predicateToken.lexeme[0] != 'p' && predicateToken.lexeme[0] != 'P')
                || predicateToken.lexeme[1] < '0' || predicateToken.lexeme[1] > '3')
            {
                fail(predicateToken, "expected predicate register p0-p3");
            }
            predicate = static_cast<PredicateIndex>(predicateToken.lexeme[1] - '0');
            ++position;
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.NE");
            ++position;
            auto registerOperand = [&]()
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, "expected GPR register r0-r31");
                ++position;
                return index;
            };
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.NE");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after SETP.NE");
            }
            program.instructions.push_back({Opcode::SETP_NE, {predicate, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isSetpLt)
        {
            const Token& predicateToken = tokens[position];
            PredicateIndex predicate{};
            if (predicateToken.kind != TokenKind::Identifier || predicateToken.lexeme.size() != 2
                || (predicateToken.lexeme[0] != 'p' && predicateToken.lexeme[0] != 'P')
                || predicateToken.lexeme[1] < '0' || predicateToken.lexeme[1] > '3')
            {
                fail(predicateToken, "expected predicate register p0-p3");
            }
            predicate = static_cast<PredicateIndex>(predicateToken.lexeme[1] - '0');
            ++position;
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.LT");
            ++position;
            auto registerOperand = [&]()
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, "expected GPR register r0-r31");
                ++position;
                return index;
            };
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.LT");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after SETP.LT");
            }
            program.instructions.push_back({Opcode::SETP_LT, {predicate, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isSetpLe)
        {
            const Token& predicateToken = tokens[position];
            PredicateIndex predicate{};
            if (predicateToken.kind != TokenKind::Identifier || predicateToken.lexeme.size() != 2
                || (predicateToken.lexeme[0] != 'p' && predicateToken.lexeme[0] != 'P')
                || predicateToken.lexeme[1] < '0' || predicateToken.lexeme[1] > '3')
            {
                fail(predicateToken, "expected predicate register p0-p3");
            }
            predicate = static_cast<PredicateIndex>(predicateToken.lexeme[1] - '0');
            ++position;
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.LE");
            ++position;
            auto registerOperand = [&]()
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, "expected GPR register r0-r31");
                ++position;
                return index;
            };
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.LE");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after SETP.LE");
            }
            program.instructions.push_back({Opcode::SETP_LE, {predicate, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isSetpGt)
        {
            const Token& predicateToken = tokens[position];
            PredicateIndex predicate{};
            if (predicateToken.kind != TokenKind::Identifier || predicateToken.lexeme.size() != 2
                || (predicateToken.lexeme[0] != 'p' && predicateToken.lexeme[0] != 'P')
                || predicateToken.lexeme[1] < '0' || predicateToken.lexeme[1] > '3')
            {
                fail(predicateToken, "expected predicate register p0-p3");
            }
            predicate = static_cast<PredicateIndex>(predicateToken.lexeme[1] - '0');
            ++position;
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.GT");
            ++position;
            auto registerOperand = [&]()
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, "expected GPR register r0-r31");
                ++position;
                return index;
            };
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.GT");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after SETP.GT");
            }
            program.instructions.push_back({Opcode::SETP_GT, {predicate, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isSetpGe)
        {
            const Token& predicateToken = tokens[position];
            PredicateIndex predicate{};
            if (predicateToken.kind != TokenKind::Identifier || predicateToken.lexeme.size() != 2
                || (predicateToken.lexeme[0] != 'p' && predicateToken.lexeme[0] != 'P')
                || predicateToken.lexeme[1] < '0' || predicateToken.lexeme[1] > '3')
            {
                fail(predicateToken, "expected predicate register p0-p3");
            }
            predicate = static_cast<PredicateIndex>(predicateToken.lexeme[1] - '0');
            ++position;
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.GE");
            ++position;
            auto registerOperand = [&]()
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, "expected GPR register r0-r31");
                ++position;
                return index;
            };
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.GE");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after SETP.GE");
            }
            program.instructions.push_back({Opcode::SETP_GE, {predicate, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isSetpFeq)
        {
            const Token& predicateToken = tokens[position];
            PredicateIndex predicate{};
            if (predicateToken.kind != TokenKind::Identifier || predicateToken.lexeme.size() != 2
                || (predicateToken.lexeme[0] != 'p' && predicateToken.lexeme[0] != 'P')
                || predicateToken.lexeme[1] < '0' || predicateToken.lexeme[1] > '3')
            {
                fail(predicateToken, "expected predicate register p0-p3");
            }
            predicate = static_cast<PredicateIndex>(predicateToken.lexeme[1] - '0');
            ++position;
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.FEQ");
            ++position;
            auto registerOperand = [&]()
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, "expected GPR register r0-r31");
                ++position;
                return index;
            };
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.FEQ");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after SETP.FEQ");
            }
            program.instructions.push_back({Opcode::SETP_FEQ, {predicate, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isSetpFne)
        {
            const Token& predicateToken = tokens[position];
            PredicateIndex predicate{};
            if (predicateToken.kind != TokenKind::Identifier || predicateToken.lexeme.size() != 2
                || (predicateToken.lexeme[0] != 'p' && predicateToken.lexeme[0] != 'P')
                || predicateToken.lexeme[1] < '0' || predicateToken.lexeme[1] > '3')
            {
                fail(predicateToken, "expected predicate register p0-p3");
            }
            predicate = static_cast<PredicateIndex>(predicateToken.lexeme[1] - '0');
            ++position;
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.FNE");
            ++position;
            auto registerOperand = [&]()
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, "expected GPR register r0-r31");
                ++position;
                return index;
            };
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.FNE");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after SETP.FNE");
            }
            program.instructions.push_back({Opcode::SETP_FNE, {predicate, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isSetpFlt)
        {
            const Token& predicateToken = tokens[position];
            PredicateIndex predicate{};
            if (predicateToken.kind != TokenKind::Identifier || predicateToken.lexeme.size() != 2
                || (predicateToken.lexeme[0] != 'p' && predicateToken.lexeme[0] != 'P')
                || predicateToken.lexeme[1] < '0' || predicateToken.lexeme[1] > '3')
            {
                fail(predicateToken, "expected predicate register p0-p3");
            }
            predicate = static_cast<PredicateIndex>(predicateToken.lexeme[1] - '0');
            ++position;
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.FLT");
            ++position;
            auto registerOperand = [&]()
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, "expected GPR register r0-r31");
                ++position;
                return index;
            };
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.FLT");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after SETP.FLT");
            }
            program.instructions.push_back({Opcode::SETP_FLT, {predicate, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isSetpFle)
        {
            const Token& predicateToken = tokens[position];
            PredicateIndex predicate{};
            if (predicateToken.kind != TokenKind::Identifier || predicateToken.lexeme.size() != 2
                || (predicateToken.lexeme[0] != 'p' && predicateToken.lexeme[0] != 'P')
                || predicateToken.lexeme[1] < '0' || predicateToken.lexeme[1] > '3')
            {
                fail(predicateToken, "expected predicate register p0-p3");
            }
            predicate = static_cast<PredicateIndex>(predicateToken.lexeme[1] - '0');
            ++position;
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.FLE");
            ++position;
            auto registerOperand = [&]()
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, "expected GPR register r0-r31");
                ++position;
                return index;
            };
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.FLE");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after SETP.FLE");
            }
            program.instructions.push_back({Opcode::SETP_FLE, {predicate, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isSetpFgt)
        {
            const Token& predicateToken = tokens[position];
            PredicateIndex predicate{};
            if (predicateToken.kind != TokenKind::Identifier || predicateToken.lexeme.size() != 2
                || (predicateToken.lexeme[0] != 'p' && predicateToken.lexeme[0] != 'P')
                || predicateToken.lexeme[1] < '0' || predicateToken.lexeme[1] > '3')
            {
                fail(predicateToken, "expected predicate register p0-p3");
            }
            predicate = static_cast<PredicateIndex>(predicateToken.lexeme[1] - '0');
            ++position;
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.FGT");
            ++position;
            auto registerOperand = [&]()
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, "expected GPR register r0-r31");
                ++position;
                return index;
            };
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.FGT");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after SETP.FGT");
            }
            program.instructions.push_back({Opcode::SETP_FGT, {predicate, source1, source2}, {}});
            if (tokens[position].kind == TokenKind::EndOfLine) ++position;
            continue;
        }
        if (isSetpFge)
        {
            const Token& predicateToken = tokens[position];
            PredicateIndex predicate{};
            if (predicateToken.kind != TokenKind::Identifier || predicateToken.lexeme.size() != 2
                || (predicateToken.lexeme[0] != 'p' && predicateToken.lexeme[0] != 'P')
                || predicateToken.lexeme[1] < '0' || predicateToken.lexeme[1] > '3')
            {
                fail(predicateToken, "expected predicate register p0-p3");
            }
            predicate = static_cast<PredicateIndex>(predicateToken.lexeme[1] - '0');
            ++position;
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.FGE");
            ++position;
            auto registerOperand = [&]()
            {
                const Token& token = tokens[position];
                GprIndex index{};
                if (token.kind != TokenKind::Identifier || !parseGpr(token.lexeme, index))
                    fail(token, "expected GPR register r0-r31");
                ++position;
                return index;
            };
            const GprIndex source1 = registerOperand();
            if (tokens[position].kind != TokenKind::Comma) fail(tokens[position], "expected comma in SETP.FGE");
            ++position;
            const GprIndex source2 = registerOperand();
            if (tokens[position].kind != TokenKind::EndOfLine
                && tokens[position].kind != TokenKind::EndOfFile)
            {
                fail(tokens[position], "unexpected operand after SETP.FGE");
            }
            program.instructions.push_back({Opcode::SETP_FGE, {predicate, source1, source2}, {}});
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
