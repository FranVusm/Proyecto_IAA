#include "dbacp/simulation.hpp"

#include <iostream>
#include <limits>
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
        instance.prerequisites = {{0, 1}};

        const dbacp::AcademicState initial = dbacp::make_state(instance, 1, {});
        dbacp::Plan plan = dbacp::make_empty_plan(instance, initial);
        plan.assigned_periods = {1, 2, 3, 1};
        const dbacp::SemesterOutcome certain = dbacp::simulate_semester(
            instance, initial, plan, {1.0, 0.5, 0.5, 0.0}, 17);
        check(certain.passed_ids == std::vector<std::string>({"A"}) &&
                  certain.failed_ids == std::vector<std::string>({"D"}),
              "q=1 aprueba, q=0 reprueba y solo se sortean cursos de t");

        const dbacp::AcademicState next = dbacp::advance_state(
            instance, initial, plan, certain.passed_ids);
        check(next.current_period == 2 &&
                  next.passed == std::vector<std::uint8_t>({1, 0, 0, 0}),
              "la simulación alimenta la transición y conserva el reprobado");

        dbacp::Plan replanned = dbacp::make_empty_plan(instance, next);
        replanned.assigned_periods = {2, 3, 3};
        const dbacp::SemesterOutcome second = dbacp::simulate_semester(
            instance, next, replanned, {1.0, 1.0, 0.0, 0.0}, 17);
        check(second.passed_ids == std::vector<std::string>({"B"}) &&
                  second.failed_ids.empty(),
              "en t+1 se sortean solo cursos de la nueva planificación");

        const std::vector<double> uncertain(4, 0.5);
        const dbacp::SemesterOutcome first_draw = dbacp::simulate_semester(
            instance, initial, plan, uncertain, 1234);
        const dbacp::SemesterOutcome repeated = dbacp::simulate_semester(
            instance, initial, plan, uncertain, 1234);
        check(first_draw.passed_ids == repeated.passed_ids &&
                  first_draw.failed_ids == repeated.failed_ids,
              "una misma semilla reproduce las aprobaciones");

        dbacp::Plan other_plan = dbacp::make_empty_plan(instance, initial);
        other_plan.assigned_periods = {1, 2, 3, 3};
        const dbacp::SemesterOutcome other_draw = dbacp::simulate_semester(
            instance, initial, other_plan, uncertain, 1234);
        const bool a_passed_first = !first_draw.passed_ids.empty() &&
                                    first_draw.passed_ids.front() == "A";
        const bool a_passed_other = !other_draw.passed_ids.empty() &&
                                    other_draw.passed_ids.front() == "A";
        check(a_passed_first == a_passed_other,
              "el sorteo de (A, t=1) no depende del resto del plan");

        bool invalid_rejected = false;
        try {
            dbacp::simulate_semester(instance, initial, plan,
                                     {1.0, 0.5, 0.5}, 1);
        } catch (const std::invalid_argument&) {
            invalid_rejected = true;
        }
        check(invalid_rejected, "se exige un q_i por curso");

        invalid_rejected = false;
        try {
            dbacp::simulate_semester(
                instance, initial, plan,
                {1.0, 0.5, std::numeric_limits<double>::quiet_NaN(), 0.0}, 1);
        } catch (const std::invalid_argument&) {
            invalid_rejected = true;
        }
        check(invalid_rejected, "se rechazan probabilidades no finitas");

        std::cout << "Simulación Bernoulli: extremos, replanificación y semilla verificados.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
