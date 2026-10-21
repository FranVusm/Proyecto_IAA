#include "dbacp/initialization.hpp"

#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error("prueba fallida: " + message);
    }
}

dbacp::Instance small_instance() {
    dbacp::Instance instance;
    instance.periods = 3;
    instance.min_credits = 1;
    instance.max_credits = 4;
    instance.min_courses = 1;
    instance.max_courses = 2;
    instance.courses = {{"A", 4}, {"B", 3}, {"C", 2}, {"D", 1}};
    instance.prerequisites = {{0, 1}};
    return instance;
}

}  // namespace

int main() {
    try {
        const dbacp::Instance instance = small_instance();
        const dbacp::AcademicState state = dbacp::make_state(instance, 1, {});
        dbacp::Plan damaged = dbacp::make_empty_plan(instance, state);
        damaged.assigned_periods = {1, 2, 2, 3};  // Cargas 4, 5, 1.
        const std::vector<std::size_t> original_courses = damaged.course_indices;
        std::mt19937_64 random(1);
        const dbacp::RepairResult repaired =
            dbacp::repair_plan(instance, state, damaged, random, 20, 500);
        check(repaired.solution.evaluation.feasible, "la reparación alcanza factibilidad");
        check(repaired.solution.plan.course_indices == original_courses,
              "la reparación conserva todos los cursos sin duplicarlos");
        check(repaired.solution.evaluation.structural_violations == 0,
              "la reparación conserva los prerrequisitos");
        check(dbacp::evaluate_plan(instance, state, repaired.solution.plan).feasible,
              "validador independiente confirma el plan reparado");
        check(repaired.evaluations <= 500 && repaired.accepted_changes > 0,
              "la reparación respeta el presupuesto y realiza cambios");

        dbacp::Instance swap_instance;
        swap_instance.periods = 2;
        swap_instance.min_credits = 5;
        swap_instance.max_credits = 5;
        swap_instance.min_courses = 2;
        swap_instance.max_courses = 2;
        swap_instance.courses = {{"P", 4}, {"Q", 2}, {"R", 3}, {"S", 1}};
        const dbacp::AcademicState swap_state =
            dbacp::make_state(swap_instance, 1, {});
        dbacp::Plan swap_plan = dbacp::make_empty_plan(swap_instance, swap_state);
        swap_plan.assigned_periods = {1, 1, 2, 2};  // Cargas 6 y 4.
        const dbacp::RepairResult swapped = dbacp::repair_plan(
            swap_instance, swap_state, swap_plan, random, 10, 100);
        check(swapped.solution.evaluation.feasible &&
                  swapped.solution.evaluation.maximum_load == 5 &&
                  swapped.solution.plan.course_indices == swap_plan.course_indices,
              "intercambio de cursos repara un caso sin movimiento individual útil");

        dbacp::Plan bad_count = swap_plan;
        bad_count.assigned_periods = {1, 2, 2, 2};
        check(dbacp::evaluate_plan(swap_instance, swap_state, bad_count)
                      .course_violation == 2,
              "cantidad insuficiente y excesiva de cursos detectada");

        dbacp::Instance impossible;
        impossible.periods = 2;
        impossible.min_credits = 5;
        impossible.max_credits = 5;
        impossible.min_courses = 1;
        impossible.max_courses = 2;
        impossible.courses = {{"X", 5}, {"Y", 5}, {"Z", 1}};
        const dbacp::AcademicState impossible_state =
            dbacp::make_state(impossible, 1, {});
        dbacp::Plan impossible_plan =
            dbacp::make_empty_plan(impossible, impossible_state);
        impossible_plan.assigned_periods = {1, 2, 2};
        const dbacp::RepairResult failed = dbacp::repair_plan(
            impossible, impossible_state, impossible_plan, random, 20, 500);
        check(!failed.solution.evaluation.feasible &&
                  failed.solution.evaluation.structural_violations == 0 &&
                  failed.solution.plan.course_indices ==
                      impossible_plan.course_indices,
              "la reparación no declara factible un estado imposible");
        const dbacp::InitializationResult blocked = dbacp::initialize_plans(
            impossible, impossible_state, {1, 5, 20, 2, 500});
        check(blocked.feasible_solutions.empty() && !blocked.diagnostics.empty() &&
                  blocked.evaluations == 0,
              "el prechequeo detecta créditos globales imposibles");

        // Un plan ejecutado en t=1 aprueba A y reprueba D.
        dbacp::Instance dynamic_instance = instance;
        dynamic_instance.max_credits = 5;
        const dbacp::AcademicState dynamic_start =
            dbacp::make_state(dynamic_instance, 1, {});
        dbacp::Plan executed = dbacp::make_empty_plan(
            dynamic_instance, dynamic_start);
        executed.assigned_periods = {1, 2, 3, 1};
        const dbacp::AcademicState dynamic = dbacp::advance_state(
            dynamic_instance, dynamic_start, executed, {"A"});
        const dbacp::InitializationOptions options{7, 10, 30, 2, 3000};
        const dbacp::InitializationResult initialized =
            dbacp::initialize_plans(dynamic_instance, dynamic, options);
        const dbacp::InitializationResult repeated =
            dbacp::initialize_plans(dynamic_instance, dynamic, options);
        check(!initialized.feasible_solutions.empty(),
              "replanificación con curso reprobado pendiente");
        check(initialized.evaluations == repeated.evaluations &&
                  initialized.feasible_solutions.front().plan.assigned_periods ==
                      repeated.feasible_solutions.front().plan.assigned_periods,
              "la misma semilla reproduce la inicialización");
        for (const auto& solution : initialized.feasible_solutions) {
            check(solution.evaluation.feasible &&
                      solution.evaluation.structural_violations == 0 &&
                      solution.plan.course_indices ==
                          std::vector<std::size_t>({1, 2, 3}),
                  "cada plan dinámico conserva cursos pendientes y prerrequisitos");
            check(dbacp::evaluate_plan(dynamic_instance, dynamic,
                                       solution.plan).feasible,
                  "cada replanificación supera el validador independiente");
        }

        const dbacp::AcademicState late = dbacp::make_state(instance, 3, {"A"});
        const dbacp::InitializationResult no_horizon =
            dbacp::initialize_plans(instance, late, options);
        check(no_horizon.feasible_solutions.empty() &&
                  !no_horizon.diagnostics.empty(),
              "el horizonte insuficiente se reporta");

        const dbacp::AcademicState last_period =
            dbacp::make_state(instance, 3, {"A", "B"});
        dbacp::Plan last_plan = dbacp::make_empty_plan(instance, last_period);
        last_plan.assigned_periods = {3, 3};
        check(dbacp::evaluate_plan(instance, last_period, last_plan).feasible,
              "último semestre cursable");
        const dbacp::AcademicState last_failed =
            dbacp::advance_state(instance, last_period, last_plan, {});
        const dbacp::InitializationResult expired =
            dbacp::initialize_plans(instance, last_failed, options);
        check(last_failed.current_period == 4 &&
                  expired.feasible_solutions.empty() &&
                  !expired.diagnostics.empty(),
              "reprobación en T se reporta sin extender horizonte");

        const dbacp::InitializationResult tight_budget =
            dbacp::initialize_plans(dynamic_instance, dynamic,
                                    {7, 10, 30, 2, 1});
        check(tight_budget.evaluations == 1 && tight_budget.attempts_used == 1,
              "el presupuesto limita las evaluaciones completas");

        std::cout << "Inicialización: reparación, infactibilidad y dinámica verificadas.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
