#include "dbacp/initialization.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace dbacp {
namespace {

struct Windows {
    std::vector<int> earliest;
    std::vector<int> latest;
    std::vector<std::vector<std::size_t>> predecessors;
    std::vector<std::vector<std::size_t>> successors;
    std::vector<int> indegree;
    std::vector<std::string> problems;
};

int violation(int value, int minimum, int maximum) {
    return std::max(0, value - maximum) + std::max(0, minimum - value);
}

int upper_excess(int value, int maximum) {
    return std::max(0, value - maximum);
}

Windows analyze_windows(const Instance& instance, const AcademicState& state,
                        const Plan& empty_plan) {
    const std::size_t n = instance.courses.size();
    Windows windows;
    windows.earliest.assign(n, state.current_period);
    windows.latest.assign(n, instance.periods);
    windows.predecessors.resize(n);
    windows.successors.resize(n);
    windows.indegree.assign(n, 0);

    for (const auto& edge : instance.prerequisites) {
        if (!state.passed[edge.first] && !state.passed[edge.second]) {
            windows.predecessors[edge.second].push_back(edge.first);
            windows.successors[edge.first].push_back(edge.second);
            ++windows.indegree[edge.second];
        }
    }

    std::queue<std::size_t> ready;
    for (const std::size_t course : empty_plan.course_indices) {
        if (windows.indegree[course] == 0) {
            ready.push(course);
        }
    }
    std::vector<int> degree = windows.indegree;
    std::vector<std::size_t> topological;
    while (!ready.empty()) {
        const std::size_t course = ready.front();
        ready.pop();
        topological.push_back(course);
        for (const std::size_t next : windows.successors[course]) {
            windows.earliest[next] =
                std::max(windows.earliest[next], windows.earliest[course] + 1);
            if (--degree[next] == 0) {
                ready.push(next);
            }
        }
    }
    if (topological.size() != empty_plan.course_indices.size()) {
        windows.problems.push_back("los prerrequisitos pendientes contienen un ciclo");
        return windows;
    }
    for (auto it = topological.rbegin(); it != topological.rend(); ++it) {
        for (const std::size_t next : windows.successors[*it]) {
            windows.latest[*it] = std::min(windows.latest[*it],
                                            windows.latest[next] - 1);
        }
    }

    const std::int64_t periods = instance.periods - state.current_period + 1;
    const std::int64_t pending_count =
        static_cast<std::int64_t>(empty_plan.course_indices.size());
    std::int64_t pending_credits = 0;
    for (const std::size_t course : empty_plan.course_indices) {
        pending_credits += instance.courses[course].credits;
        if (windows.earliest[course] > windows.latest[course]) {
            windows.problems.push_back("sin semestre posible por prerrequisitos: " +
                                       instance.courses[course].id);
        }
    }

    if (pending_count < periods * instance.min_courses ||
        pending_count > periods * instance.max_courses) {
        windows.problems.push_back("cantidad de cursos pendientes incompatible con " +
                                   std::to_string(periods) + " períodos restantes");
    }
    if (pending_credits < periods * instance.min_credits ||
        pending_credits > periods * instance.max_credits) {
        windows.problems.push_back("créditos pendientes incompatibles con " +
                                   std::to_string(periods) + " períodos restantes");
    }

    for (int period = state.current_period; period <= instance.periods; ++period) {
        int possible_count = 0;
        int possible_credits = 0;
        int mandatory_count = 0;
        int mandatory_credits = 0;
        for (const std::size_t course : empty_plan.course_indices) {
            if (windows.earliest[course] <= period && period <= windows.latest[course]) {
                ++possible_count;
                possible_credits += instance.courses[course].credits;
            }
            if (windows.earliest[course] == period &&
                windows.latest[course] == period) {
                ++mandatory_count;
                mandatory_credits += instance.courses[course].credits;
            }
        }
        if (possible_count < instance.min_courses ||
            possible_credits < instance.min_credits) {
            windows.problems.push_back("período " + std::to_string(period) +
                                       ": cursos posibles insuficientes para sus mínimos");
        }
        if (mandatory_count > instance.max_courses ||
            mandatory_credits > instance.max_credits) {
            windows.problems.push_back("período " + std::to_string(period) +
                                       ": cursos obligatorios superan sus máximos");
        }
    }
    return windows;
}

Plan construct_plan(const Instance& instance, const AcademicState& state,
                    const Plan& empty_plan, const Windows& windows,
                    std::size_t rcl_size, std::mt19937_64& random,
                    const PheromoneMatrix* pheromone = nullptr,
                    double alpha = 0.0, double beta = 0.0) {
    Plan plan = empty_plan;
    const std::size_t n = instance.courses.size();
    std::vector<std::size_t> position(n, n);
    for (std::size_t k = 0; k < plan.course_indices.size(); ++k) {
        position[plan.course_indices[k]] = k;
    }

    std::vector<int> assigned(n, 0);
    std::vector<int> degree = windows.indegree;
    const std::size_t periods =
        static_cast<std::size_t>(instance.periods - state.current_period + 1);
    std::vector<int> loads(periods, 0);
    std::vector<int> counts(periods, 0);
    std::vector<std::size_t> ready;
    for (const std::size_t course : plan.course_indices) {
        if (degree[course] == 0) {
            ready.push_back(course);
        }
    }

    for (std::size_t step = 0; step < plan.course_indices.size(); ++step) {
        std::sort(ready.begin(), ready.end(), [&](std::size_t left, std::size_t right) {
            const int left_width = windows.latest[left] - windows.earliest[left];
            const int right_width = windows.latest[right] - windows.earliest[right];
            if (left_width != right_width) {
                return left_width < right_width;
            }
            if (windows.successors[left].size() != windows.successors[right].size()) {
                return windows.successors[left].size() > windows.successors[right].size();
            }
            return instance.courses[left].credits > instance.courses[right].credits;
        });
        const std::size_t choice =
            std::uniform_int_distribution<std::size_t>(
                0, std::min(rcl_size, ready.size()) - 1)(random);
        const std::size_t course = ready[choice];
        ready.erase(ready.begin() + static_cast<std::ptrdiff_t>(choice));

        int earliest = windows.earliest[course];
        for (const std::size_t prerequisite : windows.predecessors[course]) {
            earliest = std::max(earliest, assigned[prerequisite] + 1);
        }
        if (earliest > windows.latest[course]) {
            throw std::logic_error("rango constructivo vacío para " +
                                   instance.courses[course].id);
        }

        struct Candidate {
            int period;
            int upper_delta;
            int total_delta;
            int projected_load;
        };
        std::vector<Candidate> candidates;
        for (int period = earliest; period <= windows.latest[course]; ++period) {
            const std::size_t offset = static_cast<std::size_t>(period - state.current_period);
            const int old_load = loads[offset];
            const int new_load = old_load + instance.courses[course].credits;
            const int old_count = counts[offset];
            const int new_count = old_count + 1;
            const int upper_delta =
                upper_excess(new_load, instance.max_credits) -
                    upper_excess(old_load, instance.max_credits) +
                upper_excess(new_count, instance.max_courses) -
                    upper_excess(old_count, instance.max_courses);
            const int total_delta =
                violation(new_load, instance.min_credits, instance.max_credits) -
                    violation(old_load, instance.min_credits, instance.max_credits) +
                violation(new_count, instance.min_courses, instance.max_courses) -
                    violation(old_count, instance.min_courses, instance.max_courses);
            candidates.push_back({period, upper_delta, total_delta, new_load});
        }
        std::sort(candidates.begin(), candidates.end(),
                  [](const Candidate& left, const Candidate& right) {
                      return std::tie(left.upper_delta, left.total_delta,
                                      left.projected_load) <
                             std::tie(right.upper_delta, right.total_delta,
                                      right.projected_load);
                  });
        std::size_t selected = 0;
        if (pheromone == nullptr) {
            selected = std::uniform_int_distribution<std::size_t>(
                0, std::min(rcl_size, candidates.size()) - 1)(random);
        } else {
            std::vector<double> log_weights;
            log_weights.reserve(candidates.size());
            for (std::size_t rank = 0; rank < candidates.size(); ++rank) {
                const double tau = (*pheromone)[course][candidates[rank].period];
                // eta = 1/(rank+1) reutiliza el orden heurístico greedy.
                log_weights.push_back(alpha * std::log(tau) -
                                      beta * std::log(static_cast<double>(rank + 1)));
            }
            const double maximum = *std::max_element(log_weights.begin(),
                                                      log_weights.end());
            std::vector<double> weights;
            weights.reserve(log_weights.size());
            for (const double value : log_weights) {
                weights.push_back(std::exp(value - maximum));
            }
            selected = std::discrete_distribution<std::size_t>(
                weights.begin(), weights.end())(random);
        }
        const int period = candidates[selected].period;
        assigned[course] = period;
        plan.assigned_periods[position[course]] = period;
        const std::size_t offset = static_cast<std::size_t>(period - state.current_period);
        loads[offset] += instance.courses[course].credits;
        ++counts[offset];

        for (const std::size_t next : windows.successors[course]) {
            if (--degree[next] == 0) {
                ready.push_back(next);
            }
        }
    }
    return plan;
}

}  // namespace

RepairResult repair_plan(const Instance& instance, const AcademicState& state,
                         Plan plan, std::mt19937_64& random,
                         std::size_t max_steps, std::size_t evaluation_budget) {
    if (evaluation_budget == 0) {
        throw std::invalid_argument("la reparación requiere presupuesto de evaluaciones");
    }
    RepairResult result;
    result.solution.plan = std::move(plan);
    result.solution.evaluation = evaluate_plan(instance, state, result.solution.plan);
    result.evaluations = 1;
    if (!result.solution.evaluation.complete ||
        result.solution.evaluation.structural_violations != 0) {
        throw std::invalid_argument("la reparación requiere un plan completo y estructuralmente válido");
    }

    std::vector<std::size_t> order(result.solution.plan.course_indices.size());
    std::iota(order.begin(), order.end(), 0);
    std::vector<int> periods;
    for (int period = state.current_period; period <= instance.periods; ++period) {
        periods.push_back(period);
    }

    for (std::size_t step = 0; step < max_steps &&
                               result.evaluations < evaluation_budget &&
                               !result.solution.evaluation.feasible; ++step) {
        std::vector<bool> offending(periods.size(), false);
        for (std::size_t offset = 0; offset < periods.size(); ++offset) {
            const int credits = result.solution.evaluation.credits_per_period[offset];
            const int count = result.solution.evaluation.courses_per_period[offset];
            offending[offset] = credits < instance.min_credits ||
                                credits > instance.max_credits ||
                                count < instance.min_courses ||
                                count > instance.max_courses;
        }
        std::shuffle(order.begin(), order.end(), random);
        std::shuffle(periods.begin(), periods.end(), random);
        InitializedSolution best = result.solution;
        bool improvement = false;

        for (const std::size_t k : order) {
            const FeasiblePeriodRange range = feasible_period_range(
                instance, state, result.solution.plan, k);
            const int old_period = result.solution.plan.assigned_periods[k];
            const std::size_t old_offset =
                static_cast<std::size_t>(old_period - state.current_period);
            for (const int period : periods) {
                const std::size_t new_offset =
                    static_cast<std::size_t>(period - state.current_period);
                if (period == old_period ||
                    period < range.earliest || period > range.latest) {
                    continue;
                }
                if (!offending[old_offset] && !offending[new_offset]) {
                    continue;
                }
                Plan candidate = result.solution.plan;
                candidate.assigned_periods[k] = period;
                PlanEvaluation evaluation = evaluate_plan(instance, state, candidate);
                ++result.evaluations;
                if (better_plan(evaluation, best.evaluation)) {
                    best = {std::move(candidate), std::move(evaluation)};
                    improvement = true;
                }
                if (result.evaluations >= evaluation_budget) {
                    break;
                }
            }
            if (result.evaluations >= evaluation_budget) {
                break;
            }
        }

        if (!improvement && result.evaluations < evaluation_budget) {
            for (std::size_t a = 0; a < order.size(); ++a) {
                for (std::size_t b = a + 1; b < order.size(); ++b) {
                    const std::size_t left = order[a];
                    const std::size_t right = order[b];
                    if (result.solution.plan.assigned_periods[left] ==
                        result.solution.plan.assigned_periods[right]) {
                        continue;
                    }
                    const std::size_t left_offset = static_cast<std::size_t>(
                        result.solution.plan.assigned_periods[left] -
                        state.current_period);
                    const std::size_t right_offset = static_cast<std::size_t>(
                        result.solution.plan.assigned_periods[right] -
                        state.current_period);
                    if (!offending[left_offset] && !offending[right_offset]) {
                        continue;
                    }
                    Plan candidate = result.solution.plan;
                    std::swap(candidate.assigned_periods[left],
                              candidate.assigned_periods[right]);
                    PlanEvaluation evaluation = evaluate_plan(instance, state, candidate);
                    ++result.evaluations;
                    if (better_plan(evaluation, best.evaluation)) {
                        best = {std::move(candidate), std::move(evaluation)};
                        improvement = true;
                    }
                    if (result.evaluations >= evaluation_budget) {
                        break;
                    }
                }
                if (result.evaluations >= evaluation_budget) {
                    break;
                }
            }
        }

        if (!improvement) {
            break;
        }
        result.solution = std::move(best);
        ++result.accepted_changes;
    }
    return result;
}

InitializationResult initialize_plans(const Instance& instance,
                                      const AcademicState& state,
                                      const InitializationOptions& options) {
    if (options.attempts == 0 || options.restricted_candidate_list == 0 ||
        options.evaluation_budget == 0) {
        throw std::invalid_argument("attempts, rcl y evaluation_budget deben ser positivos");
    }
    InitializationResult result;
    Plan empty_plan;
    try {
        empty_plan = make_empty_plan(instance, state);
    } catch (const std::invalid_argument& error) {
        result.diagnostics.push_back(error.what());
        return result;
    }
    if (empty_plan.course_indices.empty() && state.current_period == instance.periods + 1) {
        InitializedSolution terminal{empty_plan, evaluate_plan(instance, state, empty_plan)};
        result.feasible_solutions.push_back(std::move(terminal));
        result.evaluations = 1;
        return result;
    }

    const Windows windows = analyze_windows(instance, state, empty_plan);
    if (!windows.problems.empty()) {
        result.diagnostics = windows.problems;
        return result;
    }

    std::mt19937_64 random(options.seed);
    std::set<std::vector<int>> unique;
    while (result.attempts_used < options.attempts &&
           result.evaluations < options.evaluation_budget) {
        Plan plan = construct_plan(instance, state, empty_plan, windows,
                                   options.restricted_candidate_list, random);
        RepairResult repaired = repair_plan(instance, state, std::move(plan), random,
            options.repair_steps, options.evaluation_budget - result.evaluations);
        result.evaluations += repaired.evaluations;
        ++result.attempts_used;

        if (repaired.solution.evaluation.feasible) {
            if (unique.insert(repaired.solution.plan.assigned_periods).second) {
                result.feasible_solutions.push_back(std::move(repaired.solution));
            }
        } else if (!result.best_infeasible ||
                   better_plan(repaired.solution.evaluation,
                               result.best_infeasible->evaluation)) {
            result.best_infeasible = std::move(repaired.solution);
        }
    }

    if (result.feasible_solutions.empty()) {
        result.diagnostics.push_back(
            "no se encontró un plan factible con los intentos y evaluaciones disponibles; "
            "esto no demuestra que sea imposible");
    }
    return result;
}

Plan construct_ant_plan(const Instance& instance, const AcademicState& state,
                        const PheromoneMatrix& pheromone, double alpha,
                        double beta, std::size_t rcl_size,
                        std::mt19937_64& random) {
    if (!std::isfinite(alpha) || alpha < 0.0 || !std::isfinite(beta) ||
        beta < 0.0 || alpha > 100.0 || beta > 100.0 || rcl_size == 0) {
        throw std::invalid_argument("alpha, beta y rcl inválidos para una hormiga");
    }
    if (pheromone.size() != instance.courses.size()) {
        throw std::invalid_argument("matriz de feromonas de tamaño incorrecto");
    }
    for (const auto& row : pheromone) {
        if (row.size() != static_cast<std::size_t>(instance.periods + 1) ||
            std::any_of(row.begin() + 1, row.end(), [](double tau) {
                return !std::isfinite(tau) || tau <= 0.0;
            })) {
            throw std::invalid_argument("feromonas deben ser positivas y finitas");
        }
    }
    const Plan empty_plan = make_empty_plan(instance, state);
    const Windows windows = analyze_windows(instance, state, empty_plan);
    if (!windows.problems.empty()) {
        throw std::invalid_argument(windows.problems.front());
    }
    return construct_plan(instance, state, empty_plan, windows, rcl_size,
                          random, &pheromone, alpha, beta);
}

}  // namespace dbacp
