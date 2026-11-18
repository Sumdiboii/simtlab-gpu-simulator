#pragma once

#include <cstdint>

namespace simtlab
{

inline constexpr std::uint8_t GPR_COUNT = 32;
inline constexpr std::uint8_t PREDICATE_COUNT = 4;
inline constexpr std::uint8_t MAX_GPR_INDEX = GPR_COUNT - 1;
inline constexpr std::uint8_t MAX_PREDICATE_INDEX = PREDICATE_COUNT - 1;

enum class GprIndex : std::uint8_t
{
    R0, R1, R2, R3, R4, R5, R6, R7,
    R8, R9, R10, R11, R12, R13, R14, R15,
    R16, R17, R18, R19, R20, R21, R22, R23,
    R24, R25, R26, R27, R28, R29, R30, R31
};

enum class PredicateIndex : std::uint8_t
{
    P0, P1, P2, P3
};

} // namespace simtlab
