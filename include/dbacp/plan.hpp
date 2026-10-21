#pragma once

#include "dbacp/instance.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace dbacp {

// Estado académico al comenzar el período t.
struct AcademicState {
    int current_period = 1;
    // passed[i] = 1 si el curso i de Instance::courses ya fue aprobado.
    std::vector<std::uint8_t> passed;
};

// Representación X_t: posición k = curso pendiente course_indices[k];
// assigned_periods[k] = x_i. El valor 0 significa "aún sin asignar" solo
// mientras una hormiga construye su solución.
struct Plan {
    std::vector<std::size_t> course_indices;
    std::vector<int> assigned_periods;
};

struct PlanEvaluation {
    bool complete = false;
    bool feasible = false;
    int maximum_load = -1;
    double squared_balance_error = 0.0;
    // Equivale a ordenar por E(X_t) cuando U_t y P_t son fijos, sin redondeo.
    std::int64_t sum_squared_loads = 0;
    int credit_violation = 0;  // V_L: créditos fuera de [L_min, L_max].
    int course_violation = 0;  // V_N: cursos fuera de [N_min, N_max].
    int structural_violations = 0;  // Horizonte o prerrequisitos.
    // Posición 0 = período t, posición 1 = t+1, etc.
    std::vector<int> credits_per_period;
    std::vector<int> courses_per_period;
    std::vector<std::string> violations;
};

struct FeasiblePeriodRange {
    int earliest;
    int latest;
};

AcademicState make_state(const Instance& instance, int current_period,
                         const std::vector<std::string>& passed_ids);

Plan make_empty_plan(const Instance& instance, const AcademicState& state);

// Evalúa un plan completo según el modelo: restricciones duras y objetivo
// lexicográfico (carga máxima, luego error cuadrático de balance).
PlanEvaluation evaluate_plan(const Instance& instance, const AcademicState& state,
                             const Plan& plan);

// Para un plan completo, calcula el intervalo que mantiene los
// prerrequisitos si se mueve una sola asignatura. Puede quedar vacío.
FeasiblePeriodRange feasible_period_range(const Instance& instance,
                                          const AcademicState& state,
                                          const Plan& plan,
                                          std::size_t course_position);

// Actualiza el estado al terminar t. Solo acepta aprobaciones de cursos
// programados en t de un plan factible; los demás cursados reprueban.
AcademicState advance_state(const Instance& instance, const AcademicState& state,
                            const Plan& plan,
                            const std::vector<std::string>& passed_this_period);

// Solo compara planes completos y estructuralmente válidos del mismo estado.
// Factibles primero; infactibles por V_L+V_N; factibles por carga y E.
bool better_plan(const PlanEvaluation& candidate,
                 const PlanEvaluation& incumbent);

// Comparar únicamente planes de la misma instancia y del mismo estado t.
bool better_feasible_plan(const PlanEvaluation& candidate,
                          const PlanEvaluation& incumbent);

}  // namespace dbacp
