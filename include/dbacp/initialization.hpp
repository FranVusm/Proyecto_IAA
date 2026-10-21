#pragma once

#include "dbacp/plan.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace dbacp {

struct InitializationOptions {
    std::uint64_t seed;
    std::size_t attempts;
    std::size_t repair_steps;
    std::size_t restricted_candidate_list;
    std::size_t evaluation_budget;
};

struct InitializedSolution {
    Plan plan;
    PlanEvaluation evaluation;
};

struct RepairResult {
    InitializedSolution solution;
    std::size_t evaluations = 0;
    std::size_t accepted_changes = 0;
};

struct InitializationResult {
    std::vector<InitializedSolution> feasible_solutions;
    std::optional<InitializedSolution> best_infeasible;
    std::vector<std::string> diagnostics;
    std::size_t attempts_used = 0;
    std::size_t evaluations = 0;
};

// pheromone[i][p] representa la decisión de cursar i en el período p.
using PheromoneMatrix = std::vector<std::vector<double>>;

// Solo mueve/intercambia cursos respetando horizonte y prerrequisitos.
// Una reparación puede terminar aún infactible; nunca se presenta como válida.
RepairResult repair_plan(const Instance& instance, const AcademicState& state,
                         Plan plan, std::mt19937_64& random,
                         std::size_t max_steps, std::size_t evaluation_budget);

// Construye varios planes con greedy aleatorizado y reparación. MMAS usa
// estos planes como punto de partida y comparte la construcción restringida.
InitializationResult initialize_plans(const Instance& instance,
                                      const AcademicState& state,
                                      const InitializationOptions& options);

// Comparte la construcción restringida de la inicialización, pero elige
// períodos con pesos tau^alpha * eta^beta. eta = 1 / rango heurístico.
Plan construct_ant_plan(const Instance& instance, const AcademicState& state,
                        const PheromoneMatrix& pheromone, double alpha,
                        double beta, std::size_t rcl_size,
                        std::mt19937_64& random);

}  // namespace dbacp
