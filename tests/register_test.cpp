#include <simtlab/isa/register.hpp>

#include <type_traits>

int main()
{
    static_assert(simtlab::GPR_COUNT == 32);
    static_assert(simtlab::MAX_GPR_INDEX == 31);
    static_assert(simtlab::PREDICATE_COUNT == 4);
    static_assert(simtlab::MAX_PREDICATE_INDEX == 3);
    static_assert(!std::is_same_v<simtlab::GprIndex, simtlab::PredicateIndex>);

    constexpr simtlab::GprIndex first_gpr = simtlab::GprIndex::R0;
    constexpr simtlab::GprIndex last_gpr = simtlab::GprIndex::R31;
    constexpr simtlab::PredicateIndex first_predicate = simtlab::PredicateIndex::P0;
    constexpr simtlab::PredicateIndex last_predicate = simtlab::PredicateIndex::P3;

    if (first_gpr == last_gpr || first_predicate == last_predicate)
    {
        return 1;
    }

    return 0;
}
