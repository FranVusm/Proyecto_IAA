#include "dbacp/mmas.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <utility>

namespace dbacp {
namespace {

void set_bounds(MmasResult& result, double evaporation, double ratio) {
    if (result.best_feasible && result.best_feasible->evaluation.maximum_load > 0) {
        const double objective =
            static_cast<double>(result.best_feasible->evaluation.maximum_load);
        result.tau_max = 1.0 / (evaporation * objective);
    } else {
        result.tau_max = 1.0;
    }
    result.tau_min = ratio * result.tau_max;
    if (!std::isfinite(result.tau_max) || !std::isfinite(result.tau_min) ||
        result.tau_min <= 0.0 || result.tau_min >= result.tau_max) {
        throw std::invalid_argument("los límites de feromona no son finitos y positivos");
    }
}

void update_pheromone(const Instance& instance, const AcademicState& state,
                      double evaporation, MmasResult& result) {
    const InitializedSolution& best = *result.best_feasible;
    const double deposit = 1.0 / best.evaluation.maximum_load;
    for (std::size_t course = 0; course < instance.courses.size(); ++course) {
        for (int period = state.current_period; period <= instance.periods; ++period) {
            double& tau = result.pheromone[course][static_cast<std::size_t>(period)];
            tau *= 1.0 - evaporation;
            if (period == best.plan.assigned_periods[course]) {
                tau += deposit;
            }
            tau = std::clamp(tau, result.tau_min, result.tau_max);
        }
    }
}

}  // namespace

MmasResult run_mmas(const Instance& instance, const AcademicState& state,
                    const MmasOptions& options) {
    if (options.ants == 0 ||
        options.initialization.evaluation_budget == 0 ||
        options.initialization.evaluation_budget >= options.total_evaluation_budget ||
        !std::isfinite(options.alpha) || options.alpha < 0.0 ||
        !std::isfinite(options.beta) || options.beta < 0.0 ||
        options.alpha > 100.0 || options.beta > 100.0 ||
        !std::isfinite(options.evaporation) || options.evaporation <= 0.0 ||
        options.evaporation > 1.0 || !std::isfinite(options.tau_min_ratio) ||
        options.tau_min_ratio <= 0.0 || options.tau_min_ratio >= 1.0) {
        throw std::invalid_argument("parámetros MMAS inválidos");
    }

    MmasResult result;
    const InitializationResult initial = initialize_plans(
        instance, state, options.initialization);
    result.initialization_evaluations = initial.evaluations;
    result.evaluations = initial.evaluations;
    result.best_infeasible = initial.best_infeasible;
    for (const auto& solution : initial.feasible_solutions) {
        if (!result.best_initial_feasible ||
            better_plan(solution.evaluation,
                        result.best_initial_feasible->evaluation)) {
            result.best_initial_feasible = solution;
        }
    }
    result.best_feasible = result.best_initial_feasible;

    // Un prechequeo fallido ya prueba que este estado viola una condición
    // necesaria; no se construyen hormigas para él.
    if (initial.attempts_used == 0 && initial.feasible_solutions.empty()) {
        result.diagnostics = initial.diagnostics;
        return result;
    }
    if (state.current_period > instance.periods ||
        (result.best_feasible &&
         std::all_of(state.passed.begin(), state.passed.end(),
                     [](std::uint8_t passed) { return passed != 0; }))) {
        return result;
    }

    set_bounds(result, options.evaporation, options.tau_min_ratio);
    result.pheromone.assign(
        instance.courses.size(),
        std::vector<double>(static_cast<std::size_t>(instance.periods + 1),
                            result.tau_max));
    std::mt19937_64 random(options.initialization.seed ^
                           UINT64_C(0x9e3779b97f4a7c15));

    while (result.evaluations < options.total_evaluation_budget) {
        std::size_t ants_this_iteration = 0;
        for (std::size_t ant = 0;
             ant < options.ants && result.evaluations < options.total_evaluation_budget;
             ++ant) {
            Plan plan = construct_ant_plan(
                instance, state, result.pheromone, options.alpha,
                options.beta, options.initialization.restricted_candidate_list,
                random);
            const RepairResult repaired = repair_plan(
                instance, state, std::move(plan), random,
                options.initialization.repair_steps,
                options.total_evaluation_budget - result.evaluations);
            result.evaluations += repaired.evaluations;
            ++result.ants_built;
            ++ants_this_iteration;

            const InitializedSolution& candidate = repaired.solution;
            if (candidate.evaluation.feasible) {
                if (!result.best_feasible ||
                    better_plan(candidate.evaluation,
                                result.best_feasible->evaluation)) {
                    result.best_feasible = candidate;
                    ++result.improvements;
                }
            } else if (!result.best_infeasible ||
                       better_plan(candidate.evaluation,
                                   result.best_infeasible->evaluation)) {
                result.best_infeasible = candidate;
            }
        }
        if (ants_this_iteration == 0) {
            break;
        }
        ++result.iterations;
        if (result.best_feasible) {
            set_bounds(result, options.evaporation, options.tau_min_ratio);
            update_pheromone(instance, state, options.evaporation, result);
        }
    }

    if (!result.best_feasible) {
        result.diagnostics = initial.diagnostics;
        result.diagnostics.push_back(
            "MMAS no encontró un plan factible dentro del presupuesto; "
            "esto no demuestra infactibilidad");
    }
    return result;
}

}  // namespace dbacp
