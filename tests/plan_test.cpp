#include "dbacp/plan.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error("prueba fallida: " + message);
    }
}

}  // namespace

int main() {
    try {
        dbacp::Instance instance;
        instance.periods = 3;
        instance.min_credits = 1;
        instance.max_credits = 5;
        instance.min_courses = 1;
        instance.max_courses = 2;
        instance.courses = {{"A", 4}, {"B", 3}, {"C", 2}, {"D", 1}};
        instance.prerequisites = {{0, 1}};  // A antes de B.

        const dbacp::AcademicState initial = dbacp::make_state(instance, 1, {});
        dbacp::Plan balanced = dbacp::make_empty_plan(instance, initial);
        check(balanced.assigned_periods == std::vector<int>({0, 0, 0, 0}),
              "el vector tiene una posición por curso y comienza sin asignaciones");

        balanced.assigned_periods = {1, 2, 3, 3};  // Cargas 4, 3, 3.
        const dbacp::PlanEvaluation good =
            dbacp::evaluate_plan(instance, initial, balanced);
        check(good.complete && good.feasible && good.maximum_load == 4,
              "plan factible y carga máxima");
        check(good.credits_per_period == std::vector<int>({4, 3, 3}),
              "cargas por período");
        check(std::abs(good.squared_balance_error - 2.0 / 3.0) < 1e-9,
              "error cuadrático de balance");

        dbacp::Plan less_balanced = balanced;
        less_balanced.assigned_periods = {1, 2, 3, 2};  // Cargas 4, 4, 2.
        const dbacp::PlanEvaluation other =
            dbacp::evaluate_plan(instance, initial, less_balanced);
        check(other.feasible && other.maximum_load == good.maximum_load,
              "planes factibles con misma carga máxima");
        check(dbacp::better_feasible_plan(good, other) &&
                  !dbacp::better_feasible_plan(other, good),
              "desempate por balance cuadrático");
        check(dbacp::better_plan(good, other) &&
                  !dbacp::better_plan(other, good),
              "comparación central preserva el desempate por E");

        dbacp::Plan overloaded = balanced;
        overloaded.assigned_periods = {1, 2, 2, 2};  // Cargas 4, 6, 0.
        const dbacp::PlanEvaluation excess =
            dbacp::evaluate_plan(instance, initial, overloaded);
        check(!excess.feasible && excess.credit_violation == 2 &&
                  excess.course_violation == 2,
              "V_L y V_N incluyen excesos y déficits de cada período");
        check(dbacp::better_plan(good, excess) &&
                  !dbacp::better_plan(excess, good),
              "ningún plan infactible supera a uno factible");

        const dbacp::FeasiblePeriodRange a_range =
            dbacp::feasible_period_range(instance, initial, balanced, 0);
        const dbacp::FeasiblePeriodRange b_range =
            dbacp::feasible_period_range(instance, initial, balanced, 1);
        check(a_range.earliest == 1 && a_range.latest == 1 &&
                  b_range.earliest == 2 && b_range.latest == 3,
              "rangos según prerrequisitos y sucesores");
        dbacp::Plan moved_successor = balanced;
        moved_successor.assigned_periods[1] = 3;
        check(dbacp::feasible_period_range(instance, initial, moved_successor, 0)
                      .latest == 2,
              "el rango se recalcula después de cambiar una asignación");

        dbacp::Plan wrong_order = balanced;
        wrong_order.assigned_periods = {2, 1, 3, 3};
        check(!dbacp::evaluate_plan(instance, initial, wrong_order).feasible,
              "prerrequisito pendiente incumplido");
        check(!dbacp::better_plan(dbacp::evaluate_plan(instance, initial, wrong_order),
                                  excess),
              "un plan estructuralmente inválido nunca mejora uno válido");

        dbacp::Plan wrong_size = balanced;
        wrong_size.assigned_periods.pop_back();
        check(!dbacp::evaluate_plan(instance, initial, wrong_size).feasible,
              "X_t exige una posición por curso de la instancia");
        dbacp::Plan outside = balanced;
        outside.assigned_periods[0] = 4;
        check(dbacp::evaluate_plan(instance, initial, outside)
                      .structural_violations > 0,
              "semestre fuera del horizonte rechazado");

        dbacp::Plan incomplete = balanced;
        incomplete.assigned_periods[0] = 0;
        const dbacp::PlanEvaluation partial =
            dbacp::evaluate_plan(instance, initial, incomplete);
        check(!partial.complete && !partial.feasible,
              "una solución parcial no es factible");

        const dbacp::AcademicState next = dbacp::make_state(instance, 2, {"A"});
        dbacp::Plan replanned = dbacp::make_empty_plan(instance, next);
        check(replanned.assigned_periods == std::vector<int>({0, 0, 0, 0}),
              "el curso aprobado conserva su posición con valor cero");
        replanned.assigned_periods = {0, 2, 3, 3};
        check(replanned.assigned_periods[1] == 2 && replanned.assigned_periods[0] == 0,
              "X[i] conserva el índice del curso después de aprobar A");
        dbacp::Plan approved_reassigned = replanned;
        approved_reassigned.assigned_periods[0] = 2;
        check(!dbacp::evaluate_plan(instance, next, approved_reassigned).feasible,
              "un curso aprobado no puede recibir un período");
        const dbacp::PlanEvaluation dynamic =
            dbacp::evaluate_plan(instance, next, replanned);
        check(dynamic.feasible && dynamic.maximum_load == 3 &&
                  dynamic.squared_balance_error == 0.0,
              "replanificación con prerrequisito ya aprobado");

        dbacp::Plan executed = balanced;
        executed.assigned_periods = {1, 2, 3, 1};
        check(dbacp::evaluate_plan(instance, initial, executed).feasible,
              "plan ejecutado factible");
        const dbacp::AcademicState after_semester =
            dbacp::advance_state(instance, initial, executed, {"A"});
        check(after_semester.current_period == 2 &&
                  after_semester.passed == std::vector<std::uint8_t>({1, 0, 0, 0}),
              "A se aprueba y D reprobado sigue pendiente");
        dbacp::Plan after_failure = dbacp::make_empty_plan(instance, after_semester);
        after_failure.assigned_periods = {0, 2, 3, 3};
        check(dbacp::evaluate_plan(instance, after_semester, after_failure).feasible,
              "replanificación validada después de una reprobación");
        bool invalid_outcome_rejected = false;
        try {
            dbacp::advance_state(instance, initial, executed, {"B"});
        } catch (const std::invalid_argument&) {
            invalid_outcome_rejected = true;
        }
        check(invalid_outcome_rejected,
              "solo se pueden aprobar cursos cursados en el semestre");

        std::cout << "Representación y evaluación: casos estáticos y dinámicos verificados.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
