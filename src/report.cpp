#include "dbacp/report.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fcntl.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <utility>

namespace dbacp {
namespace {

std::string utc_timestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm utc{};
    if (gmtime_r(&now, &utc) == nullptr) {
        throw std::runtime_error("no se pudo obtener la fecha UTC del reporte");
    }
    std::ostringstream text;
    text << std::put_time(&utc, "%Y-%m-%d %H:%M:%S UTC");
    return text.str();
}

std::string safe_filename(std::string value) {
    for (char& ch : value) {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')) {
            ch = '_';
        }
    }
    return value.empty() ? "instancia" : value;
}

std::string markdown(const std::string& value) {
    std::string escaped;
    for (const char ch : value) {
        if (ch == '|' || ch == '\\' || ch == '`') {
            escaped += '\\';
        }
        if (ch == '\n' || ch == '\r') {
            escaped += ' ';
        } else {
            escaped += ch;
        }
    }
    return escaped;
}

std::string csv(const std::string& value) {
    if (value.find_first_of(",\"\n\r") == std::string::npos) {
        return value;
    }
    std::string quoted = "\"";
    for (const char ch : value) {
        if (ch == '"') {
            quoted += '"';
        }
        quoted += ch;
    }
    return quoted + '"';
}

std::string list_ids(const Instance& instance,
                     const std::vector<std::uint8_t>& flags, bool passed) {
    std::ostringstream text;
    bool first = true;
    for (std::size_t i = 0; i < flags.size(); ++i) {
        if (static_cast<bool>(flags[i]) != passed) {
            continue;
        }
        if (!first) {
            text << ", ";
        }
        text << markdown(instance.courses[i].id);
        first = false;
    }
    return first ? "ninguno" : text.str();
}

std::string list_strings(const std::vector<std::string>& ids) {
    if (ids.empty()) {
        return "ninguno";
    }
    std::ostringstream text;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            text << ", ";
        }
        text << markdown(ids[i]);
    }
    return text.str();
}

std::string machine_line(const std::string& path, const std::string& prefix) {
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind(prefix, 0) == 0) {
            const std::size_t colon = line.find(':');
            if (colon != std::string::npos) {
                const std::size_t start = line.find_first_not_of(" \t", colon + 1);
                return start == std::string::npos ? "no disponible" : line.substr(start);
            }
        }
    }
    return "no disponible";
}

void write_all(int descriptor, const std::string& content) {
    std::size_t offset = 0;
    while (offset < content.size()) {
        const ssize_t written = ::write(descriptor, content.data() + offset,
                                        content.size() - offset);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            throw std::runtime_error("no se pudo escribir resultados.csv");
        }
        offset += static_cast<std::size_t>(written);
    }
}

void append_csv_row(const std::string& path, const std::string& row) {
    const int descriptor = ::open(path.c_str(), O_CREAT | O_RDWR | O_APPEND, 0644);
    if (descriptor < 0) {
        throw std::runtime_error("no se pudo abrir '" + path + "'");
    }
    try {
        if (::flock(descriptor, LOCK_EX) != 0) {
            throw std::runtime_error("no se pudo bloquear '" + path + "'");
        }
        struct stat status{};
        if (::fstat(descriptor, &status) != 0) {
            throw std::runtime_error("no se pudo examinar '" + path + "'");
        }
        if (status.st_size == 0) {
            write_all(descriptor,
                "algoritmo,instancia,semilla,valor,factible,gap,tiempo_s,evaluaciones\n");
        }
        write_all(descriptor, row);
        if (::flock(descriptor, LOCK_UN) != 0) {
            throw std::runtime_error("no se pudo liberar '" + path + "'");
        }
        ::close(descriptor);
    } catch (...) {
        ::close(descriptor);
        throw;
    }
}

}  // namespace

RunReport::RunReport(const Instance& instance, const AcademicState& initial_state,
                     const ReportOptions& options)
    : options_(options), started_(std::chrono::steady_clock::now()) {
    namespace fs = std::filesystem;
    const fs::path directory(options_.directory);
    fs::create_directories(directory);
    const std::string stem = safe_filename(fs::path(options_.instance_path).stem().string());
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const std::string base = options_.algorithm + "_" + stem + "_seed" +
                             std::to_string(options_.seed) + "_" +
                             std::to_string(micros) + "_p" +
                             std::to_string(::getpid());
    fs::path candidate = directory / (base + ".md");
    for (std::size_t suffix = 2; fs::exists(candidate); ++suffix) {
        candidate = directory / (base + "_" + std::to_string(suffix) + ".md");
    }
    path_ = candidate.string();
    output_.open(path_);
    if (!output_) {
        throw std::runtime_error("no se pudo crear el reporte '" + path_ + "'");
    }

    output_ << "# Ejecución DBACP / " << options_.algorithm << "\n\n"
            << "- Fecha: " << utc_timestamp() << "\n"
            << "- Instancia: `" << markdown(options_.instance_path) << "`\n"
            << "- Cursos: " << instance.courses.size()
            << "; créditos: " << instance.total_credits()
            << "; prerrequisitos únicos: " << instance.prerequisites.size() << "\n"
            << "- Horizonte original T: " << instance.periods << "\n"
            << "- Cota de períodos: ";
    if (options_.period_cap) {
        output_ << *options_.period_cap;
    } else {
        output_ << "sin extensión";
    }
    output_ << "\n- Límites originales: créditos [" << instance.min_credits << ", "
            << instance.max_credits << "], cursos [" << instance.min_courses
            << ", " << instance.max_courses << "]\n"
            << "- Estado inicial: t=" << initial_state.current_period
            << "; A_t={" << list_ids(instance, initial_state.passed, true)
            << "}; U_t={" << list_ids(instance, initial_state.passed, false)
            << "}\n"
            << "- Semilla de búsqueda y simulación: " << options_.seed << "\n"
            << "- Referencia f*: ";
    if (options_.reference_value) {
        output_ << *options_.reference_value << " (entregada por CLI)";
    } else {
        output_ << "no proporcionada";
    }
    output_ << "\n"
            << "- Modo: "
            << (options_.simulate ? "simulación Bernoulli" :
                options_.manual_advance ? "avance explícito" : "planificación estática")
            << "\n\n## Parámetros\n\n"
            << "| Parámetro | Valor |\n| --- | ---: |\n"
            << "| attempts | " << options_.initialization.attempts << " |\n"
            << "| repair-steps | " << options_.initialization.repair_steps << " |\n"
            << "| rcl | " << options_.initialization.restricted_candidate_list << " |\n"
            << "| eval-budget | "
            << (options_.mmas ? options_.mmas->total_evaluation_budget :
                options_.initialization.evaluation_budget) << " |\n";
    if (options_.mmas) {
        output_ << "| init-budget | " << options_.mmas->initialization.evaluation_budget
                << " |\n| ants | " << options_.mmas->ants
                << " |\n| alpha | " << options_.mmas->alpha
                << " |\n| beta | " << options_.mmas->beta
                << " |\n| evaporation | " << options_.mmas->evaporation
                << " |\n| tau-min-ratio | " << options_.mmas->tau_min_ratio << " |\n";
    }
    if (options_.simulate) {
        output_ << "\nProbabilidades q_i (orden de la instancia):\n\n"
                << "| Curso | q_i |\n| --- | ---: |\n";
        for (std::size_t i = 0; i < instance.courses.size(); ++i) {
            output_ << "| " << markdown(instance.courses[i].id) << " | "
                    << options_.approval_probabilities[i] << " |\n";
        }
    }

    struct utsname system{};
    const bool system_ok = uname(&system) == 0;
    output_ << "\n## Entorno\n\n"
            << "- Sistema: " << (system_ok ? markdown(std::string(system.sysname) +
                                    " " + system.release + " " + system.machine) :
                                    "no disponible") << "\n"
            << "- CPU: " << markdown(machine_line("/proc/cpuinfo", "model name")) << "\n"
            << "- RAM: " << markdown(machine_line("/proc/meminfo", "MemTotal")) << "\n"
            << "- Compilador: g++ " << __VERSION__ << "; C++17, -O2\n"
            << "- Hilos usados por el algoritmo: 1\n\n"
            << "El presupuesto se aplica por planificación; en una trayectoria dinámica "
               "las evaluaciones del resumen son la suma de todas las búsquedas. "
               "El tiempo de búsqueda excluye impresión y escritura del reporte.\n";
}

void RunReport::record_search(
    const Instance& instance, const AcademicState& state, const std::string& phase,
    const std::optional<InitializedSolution>& best,
    const std::optional<InitializedSolution>& best_initial,
    const std::optional<InitializedSolution>& best_infeasible,
    const std::vector<std::string>& diagnostics, std::size_t evaluations,
    std::size_t initialization_evaluations, std::size_t ants,
    std::size_t iterations, double seconds) {
    ++searches_;
    total_evaluations_ += evaluations;
    search_seconds_ += seconds;
    if (searches_ == 1 && !options_.simulate && !options_.manual_advance && best) {
        single_value_ = best->evaluation.maximum_load;
        if (options_.reference_value) {
            single_gap_ = 100.0 *
                (static_cast<double>(*single_value_) - *options_.reference_value) /
                *options_.reference_value;
        }
    } else if (searches_ > 1) {
        single_value_.reset();
        single_gap_.reset();
    }

    output_ << "\n## Planificación " << searches_ << ": t="
            << state.current_period << " (" << phase << ")\n\n"
            << "- A_t: " << list_ids(instance, state.passed, true) << "\n"
            << "- U_t: " << list_ids(instance, state.passed, false) << "\n"
            << "- Horizonte de esta búsqueda: " << state.current_period << "–"
            << instance.periods << "\n"
            << "- Mínimos activos: " << instance.min_credits << " créditos, "
            << instance.min_courses << " cursos por período\n"
            << "- Evaluaciones: " << evaluations
            << " (inicialización: " << initialization_evaluations << ")\n"
            << "- Hormigas: " << ants << "; iteraciones: " << iterations << "\n"
            << "- Tiempo de búsqueda: " << std::fixed << std::setprecision(6)
            << seconds << " s\n"
            << "- Plan factible validado: " << (best ? "sí" : "no") << "\n"
            << "- Mejor valor de inicialización: ";
    if (best_initial) {
        output_ << "carga máxima=" << best_initial->evaluation.maximum_load
                << "; E=" << best_initial->evaluation.squared_balance_error
                << "\n";
    } else {
        output_ << "sin plan factible inicial\n";
    }
    if (!diagnostics.empty()) {
        output_ << "- Diagnósticos:\n";
        for (const auto& diagnostic : diagnostics) {
            output_ << "  - " << markdown(diagnostic) << "\n";
        }
    }
    if (!best) {
        if (best_infeasible) {
            output_ << "- Mejor intento: V_L="
                    << best_infeasible->evaluation.credit_violation
                    << ", V_N=" << best_infeasible->evaluation.course_violation
                    << "\n";
            for (const auto& violation : best_infeasible->evaluation.violations) {
                output_ << "  - " << markdown(violation) << "\n";
            }
            output_ << "\n### Vector del mejor intento infactible\n\n"
                    << "| Índice i | Curso pendiente | Créditos | Semestre asignado |\n"
                    << "| ---: | --- | ---: | ---: |\n";
            for (std::size_t course = 0; course < instance.courses.size(); ++course) {
                if (state.passed[course]) {
                    continue;
                }
                output_ << "| " << course << " | " << markdown(instance.courses[course].id)
                        << " | " << instance.courses[course].credits
                        << " | " << best_infeasible->plan.assigned_periods[course]
                        << " |\n";
            }
        }
        return;
    }

    const PlanEvaluation verified = evaluate_plan(instance, state, best->plan);
    output_ << "- Revalidación con `evaluate_plan`: "
            << (verified.feasible ? "factible" : "infactible")
            << "; carga máxima=" << verified.maximum_load
            << "; E=" << verified.squared_balance_error << "\n"
            << "- Objetivo reportado: carga máxima="
            << best->evaluation.maximum_load << "; E="
            << best->evaluation.squared_balance_error << "\n"
            << "- Gap (%): ";
    if (single_gap_ && searches_ == 1) {
        output_ << *single_gap_ << " (frente a f* entregado por CLI)\n";
    } else {
        output_ << "sin calcular; falta referencia para este estado\n";
    }
    output_ << "\n### Vector X_t\n\n"
            << "`assigned_periods[i]` es el período del curso `i`; los cursos aprobados "
               "conservan 0 y se omiten de la tabla de pendientes.\n\n"
            << "| Índice i | Curso pendiente | Créditos | Semestre asignado |\n"
            << "| ---: | --- | ---: | ---: |\n";
    for (std::size_t course = 0; course < instance.courses.size(); ++course) {
        if (state.passed[course]) {
            continue;
        }
        output_ << "| " << course << " | " << markdown(instance.courses[course].id) << " | "
                << instance.courses[course].credits << " | "
                << best->plan.assigned_periods[course] << " |\n";
    }
    output_ << "\n### Carga por semestre\n\n"
            << "| Semestre | Créditos | Cursos | Identificadores |\n"
            << "| ---: | ---: | ---: | --- |\n";
    for (int period = state.current_period; period <= instance.periods; ++period) {
        std::vector<std::string> ids;
        for (std::size_t course = 0; course < instance.courses.size(); ++course) {
            if (!state.passed[course] && best->plan.assigned_periods[course] == period) {
                ids.push_back(instance.courses[course].id);
            }
        }
        const std::size_t offset = static_cast<std::size_t>(period - state.current_period);
        output_ << "| " << period << " | " << verified.credits_per_period[offset]
                << " | " << verified.courses_per_period[offset] << " | "
                << list_strings(ids) << " |\n";
    }
}

void RunReport::record_semester(const Instance& instance, const AcademicState& state,
                                const Plan& plan, const SemesterOutcome& outcome,
                                const AcademicState& next, bool bernoulli) {
    output_ << "\n## Resultado académico del semestre " << state.current_period
            << "\n\n"
            << "- Origen: " << (bernoulli ? "Bernoulli" : "aprobaciones explícitas")
            << "\n- Cursos cursados y resultados:\n\n"
            << "| Curso | q_i | Y_it | Resultado |\n"
            << "| --- | ---: | ---: | --- |\n";
    for (std::size_t course = 0; course < instance.courses.size(); ++course) {
        if (state.passed[course] || plan.assigned_periods[course] != state.current_period) {
            continue;
        }
        const std::string& id = instance.courses[course].id;
        const bool passed = std::find(outcome.passed_ids.begin(),
                                      outcome.passed_ids.end(), id) != outcome.passed_ids.end();
        output_ << "| " << markdown(id) << " | ";
        if (bernoulli) {
            output_ << options_.approval_probabilities[course];
        } else {
            output_ << "—";
        }
        output_ << " | " << (passed ? 1 : 0) << " | "
                << (passed ? "aprobado" : "reprobado") << " |\n";
    }
    output_ << "\n- Pass_t: " << list_strings(outcome.passed_ids) << "\n"
            << "- Reprobados: " << list_strings(outcome.failed_ids) << "\n"
            << "- A_{t+1}: " << list_ids(instance, next.passed, true) << "\n"
            << "- U_{t+1}: " << list_ids(instance, next.passed, false) << "\n";
}

void RunReport::finish(int status, const std::string& reason) {
    if (finished_) {
        return;
    }
    finished_ = true;
    const double total_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started_).count();
    output_ << "\n## Resumen de ejecución\n\n"
            << "- Código de salida: " << status << "\n"
            << "- Resultado: " << markdown(reason) << "\n"
            << "- Planificaciones intentadas: " << searches_ << "\n"
            << "- Evaluaciones acumuladas: " << total_evaluations_ << "\n"
            << "- Tiempo de búsqueda acumulado: " << std::fixed
            << std::setprecision(6) << search_seconds_ << " s\n"
            << "- Tiempo total de ejecución y reporte: " << total_seconds << " s\n"
            << "- Valor para CSV: ";
    if (single_value_) {
        output_ << *single_value_;
    } else {
        output_ << (options_.simulate || options_.manual_advance ?
                    "vacío (la trayectoria tiene un objetivo por estado)" :
                    "vacío (no se validó un plan factible)");
    }
    output_ << "\n- Gap para CSV: ";
    if (single_gap_) {
        output_ << *single_gap_ << " % frente a f* entregado por CLI\n";
    } else {
        output_ << "vacío; falta referencia para este estado. "
                   "BACP12 tiene una discrepancia de 17/18 entre documentos.\n";
    }
    output_ << "- Factible para CSV: " << (status == 0 ? 1 : 0)
            << " (ejecución completada según el modo)\n";
    output_.flush();
    if (!output_) {
        throw std::runtime_error("no se pudo terminar el reporte '" + path_ + "'");
    }

    namespace fs = std::filesystem;
    const fs::path summary = fs::path(options_.directory) / "resultados.csv";
    std::ostringstream row;
    row << csv(options_.algorithm) << ','
        << csv(options_.instance_path) << ','
        << options_.seed << ',';
    if (single_value_) {
        row << *single_value_;
    }
    row << ',' << (status == 0 ? 1 : 0) << ',';
    if (single_gap_) {
        row << std::fixed << std::setprecision(6) << *single_gap_;
    }
    row << ',' << std::fixed
        << std::setprecision(6) << search_seconds_ << ','
        << total_evaluations_ << '\n';
    append_csv_row(summary.string(), row.str());
}

}  // namespace dbacp
