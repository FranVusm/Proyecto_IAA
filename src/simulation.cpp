#include "dbacp/simulation.hpp"

#include <cmath>
#include <random>
#include <stdexcept>

namespace dbacp {

SemesterOutcome simulate_semester(
    const Instance& instance, const AcademicState& state, const Plan& plan,
    const std::vector<double>& approval_probabilities,
    std::uint64_t scenario_seed) {
    if (approval_probabilities.size() != instance.courses.size()) {
        throw std::invalid_argument("se requiere un q_i por cada curso de la instancia");
    }
    for (std::size_t i = 0; i < approval_probabilities.size(); ++i) {
        const double probability = approval_probabilities[i];
        if (!std::isfinite(probability) || probability < 0.0 ||
            probability > 1.0) {
            throw std::invalid_argument("q_i fuera de [0, 1] para " +
                                        instance.courses[i].id);
        }
    }

    const PlanEvaluation evaluation = evaluate_plan(instance, state, plan);
    if (!evaluation.feasible || state.current_period > instance.periods) {
        throw std::invalid_argument(
            "la simulación requiere un plan factible en un período activo");
    }

    // Consumir una muestra por cada par (período, curso), incluso si ese
    // curso no se cursa allí, hace que el escenario no dependa del plan.
    std::mt19937_64 random(scenario_seed);
    SemesterOutcome outcome;
    for (int period = 1; period <= state.current_period; ++period) {
        std::size_t position = 0;
        for (std::size_t course = 0; course < instance.courses.size(); ++course) {
            const std::uint64_t bits = random();
            if (period != state.current_period) {
                continue;
            }
            if (position < plan.course_indices.size() &&
                plan.course_indices[position] == course) {
                if (plan.assigned_periods[position] == period) {
                    // Los 53 bits altos producen U en [0,1); 1[U < q_i]
                    // tiene distribución Bernoulli(q_i).
                    const double uniform = std::ldexp(
                        static_cast<double>(bits >> 11), -53);
                    if (uniform < approval_probabilities[course]) {
                        outcome.passed_ids.push_back(instance.courses[course].id);
                    } else {
                        outcome.failed_ids.push_back(instance.courses[course].id);
                    }
                }
                ++position;
            }
        }
    }
    return outcome;
}

}  // namespace dbacp
