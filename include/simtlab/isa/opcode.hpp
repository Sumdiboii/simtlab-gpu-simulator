#pragma once

namespace simtlab
{

enum class Opcode
{
    ADD,
    SUB,
    MUL,
    FADD,
    FMUL,
    FMA,
    AND,
    OR,
    SHL,
    SHR,
    SETP_EQ,
    SETP_NE,
    SETP_LT,
    SETP_LE,
    SETP_GT,
    SETP_GE,
    SETP_FEQ,
    SETP_FNE,
    SETP_FLT,
    SETP_FLE,
    SETP_FGT,
    SETP_FGE,
    LDG,
    STG,
    LDS,
    STS,
    BRA,
    JMP,
    BAR_SYNC,
    EXIT,
    SREG,
    MMA_M16N16K16
};

} // namespace simtlab
