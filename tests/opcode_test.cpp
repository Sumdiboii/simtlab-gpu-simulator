#include <simtlab/isa/opcode.hpp>

int main()
{
    using simtlab::Opcode;

    constexpr Opcode first = Opcode::ADD;
    constexpr Opcode compare = Opcode::SETP_EQ;
    constexpr Opcode memory = Opcode::LDG;

    if (first == compare || first == memory || compare == memory)
    {
        return 1;
    }

    return 0;
}
