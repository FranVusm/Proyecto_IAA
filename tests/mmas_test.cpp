#include "dbacp/mmas.hpp"

#include <cmath>
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
    instance.max_credits = 5;
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
        const dbacp::MmasOptions options{
            {7, 3, 10, 2, 20}, 3, 1.0, 2.0, 0.2, 0.05, 100};
        const dbacp::MmasResult result = dbacp::run_mmas(instance, state, options);
        check(result.best_feasible.has_value() &&
                  dbacp::evaluate_plan(instance, state,
                                       result.best_feasible->plan).feasible,
              "MMAS conserva un plan validado independientemente");
        check(result.initialization_evaluations <= 20 &&
                  result.evaluations <= 100 &&
                  result.evaluations > result.initialization_evaluations &&
                  result.ants_built > 0 && result.iterations > 0,
              "la búsqueda ocurre después de inicializar y respeta presupuesto global");
        check(result.tau_min > 0.0 && result.tau_min < result.tau_max,
              "límites de feromona válidos");
        bool has_high_trail = false;
        bool has_low_trail = false;
        for (const auto& row : result.pheromone) {
            for (int period = state.current_period; period <= instance.periods;
                 ++period) {
                const double tau = row[static_cast<std::size_t>(period)];
                check(std::isfinite(tau) &&
                          tau >= result.tau_min - 1e-12 &&
                          tau <= result.tau_max + 1e-12,
                      "cada feromona permanece dentro de sus límites");
                has_high_trail = has_high_trail || tau > result.tau_min + 1e-6;
                has_low_trail = has_low_trail || tau < result.tau_max - 1e-6;
            }
        }
        check(has_high_trail && has_low_trail,
              "el refuerzo y la evaporación diferencian decisiones");

        dbacp::Instance one_course;
        one_course.periods = 2;
        one_course.min_credits = 0;
        one_course.max_credits = 1;
        one_course.min_courses = 0;
        one_course.max_courses = 1;
        one_course.courses = {{"X", 1}};
        const dbacp::AcademicState one_state =
            dbacp::make_state(one_course, 1, {});
        const dbacp::PheromoneMatrix first_high{{1.0, 1000.0, 1.0}};
        const dbacp::PheromoneMatrix second_high{{1.0, 1.0, 1000.0}};
        std::mt19937_64 first_random(9);
        std::mt19937_64 second_random(9);
        int first_choices = 0;
        int second_choices = 0;
        for (int trial = 0; trial < 50; ++trial) {
            first_choices += dbacp::construct_ant_plan(
                one_course, one_state, first_high, 1.0, 0.0, 1,
                first_random).assigned_periods[0] == 1;
            second_choices += dbacp::construct_ant_plan(
                one_course, one_state, second_high, 1.0, 0.0, 1,
                second_random).assigned_periods[0] == 2;
        }
        check(first_choices >= 48 && second_choices >= 48,
              "la feromona realmente guía la elección de semestre");

        const dbacp::MmasResult repeated = dbacp::run_mmas(instance, state, options);
        check(repeated.evaluations == result.evaluations &&
                  repeated.best_feasible->plan.assigned_periods ==
                      result.best_feasible->plan.assigned_periods &&
                  repeated.pheromone == result.pheromone,
              "la semilla reproduce búsqueda y feromonas");

        dbacp::MmasOptions invalid = options;
        invalid.initialization.evaluation_budget = 100;
        bool rejected = false;
        try {
            dbacp::run_mmas(instance, state, invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected, "se reserva presupuesto para las hormigas");

        dbacp::Instance impossible = instance;
        impossible.periods = 1;
        const dbacp::AcademicState impossible_state =
            dbacp::make_state(impossible, 1, {});
        const dbacp::MmasResult blocked =
            dbacp::run_mmas(impossible, impossible_state, options);
        check(!blocked.best_feasible && blocked.ants_built == 0 &&
                  !blocked.diagnostics.empty(),
              "un estado sin horizonte se informa sin presentar solución inválida");

        const dbacp::Instance bacp12 =
            dbacp::load_instance("instances/classic/bacp12.dat");
        const dbacp::AcademicState bacp12_state =
            dbacp::make_state(bacp12, 1, {});
        const dbacp::InitializationOptions short_start{1, 1, 30, 3, 1};
        const dbacp::InitializationResult initial_only =
            dbacp::initialize_plans(bacp12, bacp12_state, short_start);
        check(initial_only.feasible_solutions.empty(),
              "presupuesto inicial corto deja un caso aún sin plan válido");
        const dbacp::MmasResult found_by_search = dbacp::run_mmas(
            bacp12, bacp12_state,
            {short_start, 5, 1.0, 2.0, 0.2, 0.05, 5000});
        check(found_by_search.best_feasible.has_value() &&
                  found_by_search.ants_built > 0 &&
                  dbacp::evaluate_plan(bacp12, bacp12_state,
                                       found_by_search.best_feasible->plan).feasible,
              "las hormigas encuentran un plan válido tras una inicialización fallida");

        std::cout << "MMAS: búsqueda, feromonas, presupuesto y reproducción verificados.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
