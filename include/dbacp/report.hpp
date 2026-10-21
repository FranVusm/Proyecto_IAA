#pragma once

#include "dbacp/initialization.hpp"
#include "dbacp/mmas.hpp"
#include "dbacp/simulation.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace dbacp {

struct ReportOptions {
    std::string directory = "results";
    std::string instance_path;
    std::string algorithm;
    std::uint64_t seed = 0;
    InitializationOptions initialization{};
    std::optional<MmasOptions> mmas;
    std::optional<int> period_cap;
    std::optional<int> reference_value;
    bool simulate = false;
    bool manual_advance = false;
    std::optional<std::uint64_t> simulation_seed;
    std::vector<double> approval_probabilities;
};

class RunReport {
public:
    RunReport(const Instance& instance, const AcademicState& initial_state,
              const ReportOptions& options);

    const std::string& path() const { return path_; }

    void record_search(const Instance& instance, const AcademicState& state,
                       const std::string& phase,
                       const std::optional<InitializedSolution>& best,
                       const std::optional<InitializedSolution>& best_infeasible,
                       const std::vector<std::string>& diagnostics,
                       std::size_t evaluations, std::size_t initialization_evaluations,
                       std::size_t ants, std::size_t iterations, double seconds);

    void record_semester(const Instance& instance, const AcademicState& state,
                         const Plan& plan, const SemesterOutcome& outcome,
                         const AcademicState& next, bool bernoulli);

    // status 0: ejecución completada; 3: sin plan/pendientes tras la cota;
    // 1: error de entrada o ejecución. El CSV usa una fila por ejecución.
    void finish(int status, const std::string& reason);

private:
    ReportOptions options_;
    std::string path_;
    std::ofstream output_;
    std::chrono::steady_clock::time_point started_;
    std::size_t total_evaluations_ = 0;
    std::size_t searches_ = 0;
    double search_seconds_ = 0.0;
    std::optional<int> single_value_;
    std::optional<double> single_gap_;
    bool finished_ = false;
};

}  // namespace dbacp
