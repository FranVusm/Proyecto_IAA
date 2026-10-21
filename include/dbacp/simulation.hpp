#pragma once

#include "dbacp/plan.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace dbacp {

struct SemesterOutcome {
    std::vector<std::string> passed_ids;
    std::vector<std::string> failed_ids;
};

// Simula Y_i^t ~ Bernoulli(q_i) solo para cursos programados en t.
// approval_probabilities[i] corresponde a Instance::courses[i].
// La semilla fija un sorteo por (período, curso), independientemente del
// plan, para compartir escenarios entre algoritmos que usen la misma semilla.
SemesterOutcome simulate_semester(
    const Instance& instance, const AcademicState& state, const Plan& plan,
    const std::vector<double>& approval_probabilities,
    std::uint64_t scenario_seed);

}  // namespace dbacp
