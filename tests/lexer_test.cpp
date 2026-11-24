#include <simtlab/isa/lexer.hpp>

#include <stdexcept>
#include <string>

int main()
{
    using namespace simtlab;
    const auto numbers = lex("+12,-0xAf,[3.25]\n");
    if (numbers.size() != 11 || numbers[0].kind != TokenKind::Plus
        || numbers[1].kind != TokenKind::IntegerLiteral || numbers[1].lexeme != "12"
        || numbers[1].line != 1 || numbers[1].column != 2
        || numbers[2].kind != TokenKind::Comma || numbers[3].kind != TokenKind::Minus
        || numbers[4].kind != TokenKind::IntegerLiteral || numbers[4].lexeme != "0xAf"
        || numbers[4].column != 6 || numbers[5].kind != TokenKind::Comma
        || numbers[6].kind != TokenKind::LeftBracket || numbers[7].kind != TokenKind::FloatLiteral
        || numbers[7].lexeme != "3.25" || numbers[7].column != 12
        || numbers[8].kind != TokenKind::RightBracket || numbers[9].kind != TokenKind::EndOfLine
        || numbers[10].kind != TokenKind::EndOfFile)
    {
        return 1;
    }
    const auto later_number = lex("\n 42");
    if (later_number.size() != 3 || later_number[1].kind != TokenKind::IntegerLiteral
        || later_number[1].line != 2
        || later_number[1].column != 2 || later_number[1].lexeme != "42")
    {
        return 1;
    }
    const char* malformed[] = {"0x", "0xG", "12abc", "1.", "1.2.3", "1e3", ".5"};
    const char* locations[] = {"1:3", "1:3", "1:3", "1:2", "1:4", "1:2", "1:1"};
    for (int i = 0; i < 7; ++i)
    {
        try
        {
            lex(malformed[i]);
        }
        catch (const std::runtime_error& error)
        {
            const std::string message = error.what();
            if (message.find("malformed numeric literal at") != std::string::npos
                && message.find(locations[i]) != std::string::npos)
            {
                continue;
            }
        }
        return 1;
    }
    const auto punctuation = lex("@!,:.[]+-\n");
    if (punctuation.size() != 11 || punctuation[0].kind != TokenKind::AtSign
        || punctuation[0].lexeme != "@" || punctuation[0].column != 1
        || punctuation[1].kind != TokenKind::Bang || punctuation[1].lexeme != "!"
        || punctuation[1].column != 2 || punctuation[2].kind != TokenKind::Comma
        || punctuation[2].lexeme != "," || punctuation[2].column != 3
        || punctuation[3].kind != TokenKind::Colon || punctuation[3].lexeme != ":"
        || punctuation[3].column != 4 || punctuation[4].kind != TokenKind::Dot
        || punctuation[4].lexeme != "." || punctuation[4].column != 5
        || punctuation[5].kind != TokenKind::LeftBracket || punctuation[5].lexeme != "["
        || punctuation[5].column != 6 || punctuation[6].kind != TokenKind::RightBracket
        || punctuation[6].lexeme != "]" || punctuation[6].column != 7
        || punctuation[7].kind != TokenKind::Plus || punctuation[7].lexeme != "+"
        || punctuation[7].column != 8 || punctuation[8].kind != TokenKind::Minus
        || punctuation[8].lexeme != "-" || punctuation[8].column != 9
        || punctuation[9].kind != TokenKind::EndOfLine || punctuation[9].line != 1
        || punctuation[10].kind != TokenKind::EndOfFile || punctuation[10].line != 2)
    {
        return 1;
    }
    for (int i = 0; i < 9; ++i)
    {
        if (punctuation[i].line != 1)
        {
            return 1;
        }
    }
    const auto later_punctuation = lex("\n@");
    if (later_punctuation.size() != 3 || later_punctuation[1].kind != TokenKind::AtSign
        || later_punctuation[1].line != 2
        || later_punctuation[1].column != 1)
    {
        return 1;
    }
    const auto dotted_mnemonic = lex("SETP.EQ\n");
    const auto label = lex("loop:\n");
    if (dotted_mnemonic.size() != 5 || label.size() != 4
        || dotted_mnemonic[0].lexeme != "SETP" || dotted_mnemonic[1].kind != TokenKind::Dot
        || dotted_mnemonic[2].lexeme != "EQ" || label[0].lexeme != "loop"
        || label[1].kind != TokenKind::Colon || label[1].column != 5)
    {
        return 1;
    }
    const auto identifiers = lex("ADD r12 p3\nLoop\n");
    if (identifiers.size() != 7 || identifiers[0].kind != TokenKind::Identifier
        || identifiers[0].lexeme != "ADD" || identifiers[0].line != 1 || identifiers[0].column != 1
        || identifiers[1].lexeme != "r12" || identifiers[1].line != 1 || identifiers[1].column != 5
        || identifiers[2].lexeme != "p3" || identifiers[2].line != 1 || identifiers[2].column != 9
        || identifiers[4].lexeme != "Loop" || identifiers[4].line != 2 || identifiers[4].column != 1
        || identifiers[6].kind != TokenKind::EndOfFile)
    {
        return 1;
    }
    const auto tokens = lex(" \t# ignored\r\n\n");
    if (tokens.size() != 3 || tokens[0].kind != TokenKind::EndOfLine
        || tokens[0].lexeme != "\r\n" || tokens[0].line != 1 || tokens[0].column != 12
        || tokens[1].kind != TokenKind::EndOfLine || tokens[1].line != 2
        || tokens[2].kind != TokenKind::EndOfFile || tokens[2].line != 3)
    {
        return 1;
    }
    const auto empty = lex("");
    const auto final_line = lex(" \t");
    if (empty.size() != 1 || empty[0].kind != TokenKind::EndOfFile
        || final_line.size() != 1 || final_line[0].kind != TokenKind::EndOfFile
        || final_line[0].column != 3)
    {
        return 1;
    }
    try
    {
        lex("$");
    }
    catch (const std::runtime_error& error)
    {
        if (std::string(error.what()).find("1:1") != std::string::npos)
        {
            return 0;
        }
    }
    return 1;
}
