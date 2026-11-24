#include <simtlab/isa/lexer.hpp>

#include <stdexcept>
#include <string>
namespace simtlab
{

std::vector<Token> lex(std::string_view source)
{
    std::vector<Token> tokens;
    std::size_t position = 0, line = 1, column = 1;
    auto error = [&](const char* reason = "unsupported character")
    {
        throw std::runtime_error(std::string(reason) + " at "
                                 + std::to_string(line) + ":" + std::to_string(column));
    };
    while (position < source.size())
    {
        const auto ch = static_cast<unsigned char>(source[position]);
        if (ch == '.' && position + 1 < source.size()
            && source[position + 1] >= '0' && source[position + 1] <= '9')
        {
            error("malformed numeric literal");
        }
        if (ch >= '0' && ch <= '9')
        {
            const auto start = position, col = column;
            auto kind = TokenKind::IntegerLiteral;
            if (ch == '0' && position + 1 < source.size() && source[position + 1] == 'x')
            {
                position += 2;
                column += 2;
                const auto digits = position;
                while (position < source.size()
                       && ((source[position] >= '0' && source[position] <= '9')
                           || (source[position] >= 'a' && source[position] <= 'f')
                           || (source[position] >= 'A' && source[position] <= 'F')))
                {
                    ++position;
                    ++column;
                }
                if (position == digits) error("malformed numeric literal");
            }
            else
            {
                while (position < source.size() && source[position] >= '0' && source[position] <= '9')
                {
                    ++position;
                    ++column;
                }
                if (position < source.size() && source[position] == '.')
                {
                    if (position + 1 == source.size() || source[position + 1] < '0'
                        || source[position + 1] > '9')
                    {
                        error("malformed numeric literal");
                    }
                    kind = TokenKind::FloatLiteral;
                    ++position;
                    ++column;
                    while (position < source.size() && source[position] >= '0' && source[position] <= '9')
                    {
                        ++position;
                        ++column;
                    }
                }
            }
            if (position < source.size())
            {
                const auto next = static_cast<unsigned char>(source[position]);
                if ((next >= 'A' && next <= 'Z') || (next >= 'a' && next <= 'z')
                    || next == '_' || next == '.') error("malformed numeric literal");
            }
            tokens.push_back({kind, std::string(source.substr(start, position - start)), line, col});
            continue;
        }
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_')
        {
            const auto start = position, col = column;
            while (position < source.size())
            {
                const auto next = static_cast<unsigned char>(source[position]);
                if (!((next >= 'A' && next <= 'Z') || (next >= 'a' && next <= 'z')
                      || (next >= '0' && next <= '9') || next == '_'))
                {
                    break;
                }
                ++position;
                ++column;
            }
            tokens.push_back({TokenKind::Identifier,
                              std::string(source.substr(start, position - start)), line, col});
            continue;
        }
        if (ch == '@' || ch == '!' || ch == ',' || ch == ':' || ch == '.' || ch == '['
            || ch == ']' || ch == '+' || ch == '-')
        {
            TokenKind kind = TokenKind::AtSign;
            switch (ch)
            {
            case '@': kind = TokenKind::AtSign; break;
            case '!': kind = TokenKind::Bang; break;
            case ',': kind = TokenKind::Comma; break;
            case ':': kind = TokenKind::Colon; break;
            case '.': kind = TokenKind::Dot; break;
            case '[': kind = TokenKind::LeftBracket; break;
            case ']': kind = TokenKind::RightBracket; break;
            case '+': kind = TokenKind::Plus; break;
            case '-': kind = TokenKind::Minus; break;
            default: error();
            }
            tokens.push_back({kind, std::string(1, source[position]), line, column});
            ++position;
            ++column;
            continue;
        }
        if (ch == ' ' || ch == '\t')
        {
            ++position;
            ++column;
            continue;
        }
        if (ch == '#')
        {
            while (position < source.size() && source[position] != '\n' && source[position] != '\r')
            {
                ++position;
                ++column;
            }
            continue;
        }
        if (ch == '\n' || ch == '\r')
        {
            const auto start = position, col = column;
            if (ch == '\r' && position + 1 < source.size() && source[position + 1] == '\n')
            {
                position += 2;
            }
            else
            {
                ++position;
            }
            tokens.push_back({TokenKind::EndOfLine, std::string(source.substr(start, position - start)), line, col});
            ++line;
            column = 1;
            continue;
        }
        error();
    }
    tokens.push_back({TokenKind::EndOfFile, "", line, column});
    return tokens;
}
} // namespace simtlab
