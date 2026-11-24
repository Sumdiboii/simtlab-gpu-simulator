#include <simtlab/isa/token.hpp>

int main()
{
    using namespace simtlab;

    const Token mnemonic{TokenKind::Identifier, "SETP", 1, 1};
    const Token dot{TokenKind::Dot, ".", 1, 5};
    const Token integer{TokenKind::IntegerLiteral, "0x10", 1, 6};
    const Token floating{TokenKind::FloatLiteral, "1.5", 1, 10};
    const Token predicate_prefix{TokenKind::AtSign, "@", 2, 1};
    const Token negation{TokenKind::Bang, "!", 2, 2};
    const Token line_end{TokenKind::EndOfLine, "\n", 2, 5};
    const Token file_end{TokenKind::EndOfFile, "", 3, 1};

    if (mnemonic.kind != TokenKind::Identifier || mnemonic.lexeme != "SETP"
        || mnemonic.line != 1 || mnemonic.column != 1
        || dot.kind != TokenKind::Dot || dot.lexeme != "." || dot.column != 5
        || integer.kind != TokenKind::IntegerLiteral || integer.lexeme != "0x10"
        || integer.line != 1 || integer.column != 6
        || floating.kind != TokenKind::FloatLiteral || floating.lexeme != "1.5"
        || floating.line != 1 || floating.column != 10
        || predicate_prefix.kind != TokenKind::AtSign || negation.kind != TokenKind::Bang
        || line_end.kind != TokenKind::EndOfLine || file_end.kind != TokenKind::EndOfFile
        || !file_end.lexeme.empty() || file_end.line != 3 || file_end.column != 1)
    {
        return 1;
    }

    return 0;
}
