#pragma once

#include "dbacp/initialization.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace dbacp {

struct MmasOptions {
    InitializationOptions initialization;  // Incluye su propio presupuesto.
    std::size_t ants;
    double alpha;
    double beta;
    double evaporation;  // Fracción rho en (0,1].
    double tau_min_ratio;  // tau_min / tau_max en (0,1).
    std::size_t total_evaluation_budget;
};

struct MmasResult {
    std::optional<InitializedSolution> best_feasible;
    std::optional<InitializedSolution> best_infeasible;
    std::vector<std::string> diagnostics;
    PheromoneMatrix pheromone;
    double tau_min = 0.0;
    double tau_max = 0.0;
    std::size_t initialization_evaluations = 0;
    std::size_t evaluations = 0;
    std::size_t ants_built = 0;
    std::size_t iterations = 0;
    std::size_t improvements = 0;
};

// Ejecuta MMAS sobre un estado fijo. Primero inicializa con greedy;
// luego construye hormigas, repara, evalúa y actualiza feromonas hasta
// agotar el presupuesto global. Solo una solución factible deposita tau.
MmasResult run_mmas(const Instance& instance, const AcademicState& state,
                    const MmasOptions& options);

}  // namespace dbacp
