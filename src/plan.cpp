#include "dbacp/plan.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace dbacp {
namespace {

void validate_state(const Instance& instance, const AcademicState& state) {
    if (state.current_period < 1 || state.current_period > instance.periods + 1) {
        throw std::invalid_argument("el período actual debe estar entre 1 y T+1");
    }
    if (state.passed.size() != instance.courses.size()) {
        throw std::invalid_argument("el estado académico no corresponde a la instancia");
    }
    if (std::any_of(state.passed.begin(), state.passed.end(),
                    [](std::uint8_t value) { return value > 1; })) {
        throw std::invalid_argument("passed solo admite valores 0 y 1");
    }
}

}  // namespace

AcademicState make_state(const Instance& instance, int current_period,
                         const std::vector<std::string>& passed_ids) {
    AcademicState state;
    state.current_period = current_period;
    state.passed.assign(instance.courses.size(), 0);
    validate_state(instance, state);

    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < instance.courses.size(); ++i) {
        index.emplace(instance.courses[i].id, i);
    }
    for (const std::string& id : passed_ids) {
        const auto found = index.find(id);
        if (found == index.end()) {
            throw std::invalid_argument("curso aprobado desconocido: " + id);
        }
        if (state.passed[found->second]) {
            throw std::invalid_argument("curso aprobado repetido: " + id);
        }
        state.passed[found->second] = 1;
    }
    return state;
}

Plan make_empty_plan(const Instance& instance, const AcademicState& state) {
    validate_state(instance, state);
    Plan plan;
    for (std::size_t i = 0; i < state.passed.size(); ++i) {
        if (!state.passed[i]) {
            plan.course_indices.push_back(i);
            plan.assigned_periods.push_back(0);
        }
    }
    if (state.current_period > instance.periods && !plan.course_indices.empty()) {
        throw std::invalid_argument(
            "sin planificación factible: quedan cursos pendientes después del período T");
    }
    return plan;
}

PlanEvaluation evaluate_plan(const Instance& instance, const AcademicState& state,
                             const Plan& plan) {
    validate_state(instance, state);
    PlanEvaluation result;
    const int remaining_periods = std::max(0, instance.periods - state.current_period + 1);
    result.credits_per_period.assign(static_cast<std::size_t>(remaining_periods), 0);
    result.courses_per_period.assign(static_cast<std::size_t>(remaining_periods), 0);

    if (plan.course_indices.size() != plan.assigned_periods.size()) {
        result.violations.push_back("el vector de cursos y el de períodos tienen tamaños distintos");
        return result;
    }

    const Plan expected = make_empty_plan(instance, state);
    if (plan.course_indices != expected.course_indices) {
        result.violations.push_back("las posiciones del vector no corresponden a U_t en orden de instancia");
        return result;
    }

    if (remaining_periods == 0 && plan.course_indices.empty()) {
        result.complete = true;
        result.feasible = true;
        result.maximum_load = 0;
        return result;
    }

    result.complete = true;
    std::vector<int> assigned_by_course(instance.courses.size(), 0);
    for (std::size_t k = 0; k < plan.course_indices.size(); ++k) {
        const std::size_t course = plan.course_indices[k];
        const int period = plan.assigned_periods[k];
        if (period == 0) {
            result.complete = false;
            result.violations.push_back("curso sin asignar: " + instance.courses[course].id);
            continue;
        }
        if (period < state.current_period || period > instance.periods) {
            ++result.structural_violations;
            result.violations.push_back("período fuera del horizonte para " +
                                        instance.courses[course].id);
            continue;
        }
        assigned_by_course[course] = period;
        const std::size_t offset = static_cast<std::size_t>(period - state.current_period);
        result.credits_per_period[offset] += instance.courses[course].credits;
        ++result.courses_per_period[offset];
    }

    if (!result.complete) {
        return result;
    }

    for (const auto& edge : instance.prerequisites) {
        const std::size_t prerequisite = edge.first;
        const std::size_t dependent = edge.second;
        if (!state.passed[prerequisite] && !state.passed[dependent] &&
            assigned_by_course[prerequisite] >= assigned_by_course[dependent]) {
            ++result.structural_violations;
            result.violations.push_back("prerrequisito incumplido: " +
                                        instance.courses[prerequisite].id + " antes de " +
                                        instance.courses[dependent].id);
        }
    }

    for (int offset = 0; offset < remaining_periods; ++offset) {
        const int period = state.current_period + offset;
        const int credits = result.credits_per_period[static_cast<std::size_t>(offset)];
        const int count = result.courses_per_period[static_cast<std::size_t>(offset)];
        result.credit_violation += std::max(0, credits - instance.max_credits) +
                                   std::max(0, instance.min_credits - credits);
        result.course_violation += std::max(0, count - instance.max_courses) +
                                   std::max(0, instance.min_courses - count);
        if (credits < instance.min_credits || credits > instance.max_credits) {
            result.violations.push_back("carga de créditos fuera de límites en período " +
                                        std::to_string(period) + ": " +
                                        std::to_string(credits) + " frente a [" +
                                        std::to_string(instance.min_credits) + ", " +
                                        std::to_string(instance.max_credits) + "]");
        }
        if (count < instance.min_courses || count > instance.max_courses) {
            result.violations.push_back("cantidad de cursos fuera de límites en período " +
                                        std::to_string(period) + ": " +
                                        std::to_string(count) + " frente a [" +
                                        std::to_string(instance.min_courses) + ", " +
                                        std::to_string(instance.max_courses) + "]");
        }
    }

    if (!result.credits_per_period.empty()) {
        result.maximum_load = *std::max_element(result.credits_per_period.begin(),
                                                result.credits_per_period.end());
        std::int64_t pending_credits = 0;
        for (const std::size_t course : plan.course_indices) {
            pending_credits += instance.courses[course].credits;
        }
        const double average = static_cast<double>(pending_credits) / remaining_periods;
        for (const int credits : result.credits_per_period) {
            const double difference = credits - average;
            result.squared_balance_error += difference * difference;
            result.sum_squared_loads += static_cast<std::int64_t>(credits) * credits;
        }
    }

    result.feasible = result.violations.empty();
    return result;
}

FeasiblePeriodRange feasible_period_range(const Instance& instance,
                                          const AcademicState& state,
                                          const Plan& plan,
                                          std::size_t course_position) {
    validate_state(instance, state);
    const Plan expected = make_empty_plan(instance, state);
    if (plan.course_indices != expected.course_indices ||
        plan.assigned_periods.size() != expected.assigned_periods.size() ||
        course_position >= plan.course_indices.size()) {
        throw std::invalid_argument("el plan no corresponde a U_t o el curso no existe");
    }

    std::vector<int> assigned_by_course(instance.courses.size(), 0);
    for (std::size_t k = 0; k < plan.course_indices.size(); ++k) {
        const int period = plan.assigned_periods[k];
        if (period < state.current_period || period > instance.periods) {
            throw std::invalid_argument("el cálculo de rangos requiere un plan completo en P_t");
        }
        assigned_by_course[plan.course_indices[k]] = period;
    }

    const std::size_t course = plan.course_indices[course_position];
    FeasiblePeriodRange range{state.current_period, instance.periods};
    for (const auto& edge : instance.prerequisites) {
        if (state.passed[edge.first] || state.passed[edge.second]) {
            continue;
        }
        if (edge.second == course) {
            range.earliest = std::max(range.earliest,
                                      assigned_by_course[edge.first] + 1);
        }
        if (edge.first == course) {
            range.latest = std::min(range.latest,
                                    assigned_by_course[edge.second] - 1);
        }
    }
    return range;
}

AcademicState advance_state(const Instance& instance, const AcademicState& state,
                            const Plan& plan,
                            const std::vector<std::string>& passed_this_period) {
    const PlanEvaluation evaluation = evaluate_plan(instance, state, plan);
    if (!evaluation.feasible || state.current_period > instance.periods) {
        throw std::invalid_argument("el avance requiere un plan factible en un período activo");
    }

    std::unordered_map<std::string, std::size_t> scheduled;
    for (std::size_t k = 0; k < plan.course_indices.size(); ++k) {
        if (plan.assigned_periods[k] == state.current_period) {
            const std::size_t course = plan.course_indices[k];
            scheduled.emplace(instance.courses[course].id, course);
        }
    }

    AcademicState next = state;
    std::unordered_set<std::string> seen;
    for (const std::string& id : passed_this_period) {
        const auto found = scheduled.find(id);
        if (found == scheduled.end()) {
            throw std::invalid_argument("curso aprobado no programado en el período actual: " + id);
        }
        if (!seen.insert(id).second) {
            throw std::invalid_argument("curso aprobado repetido: " + id);
        }
        next.passed[found->second] = 1;
    }
    ++next.current_period;
    return next;
}

bool better_plan(const PlanEvaluation& candidate,
                 const PlanEvaluation& incumbent) {
    if (!candidate.complete || candidate.structural_violations != 0) {
        return false;
    }
    if (!incumbent.complete || incumbent.structural_violations != 0) {
        return true;
    }
    if (candidate.feasible != incumbent.feasible) {
        return candidate.feasible;
    }
    if (!candidate.feasible) {
        const int candidate_violation = candidate.credit_violation +
                                        candidate.course_violation;
        const int incumbent_violation = incumbent.credit_violation +
                                        incumbent.course_violation;
        if (candidate_violation != incumbent_violation) {
            return candidate_violation < incumbent_violation;
        }
    }
    if (candidate.maximum_load != incumbent.maximum_load) {
        return candidate.maximum_load < incumbent.maximum_load;
    }
    return candidate.sum_squared_loads < incumbent.sum_squared_loads;
}

bool better_feasible_plan(const PlanEvaluation& candidate,
                          const PlanEvaluation& incumbent) {
    return candidate.feasible && better_plan(candidate, incumbent);
}

}  // namespace dbacp
