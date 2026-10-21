#include "dbacp/instance.hpp"
#include "dbacp/initialization.hpp"
#include "dbacp/mmas.hpp"
#include "dbacp/plan.hpp"
#include "dbacp/report.hpp"
#include "dbacp/simulation.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

void usage(const char* command, std::ostream& output) {
    output << "Uso: " << command
           << " \"instancia.dat\" [--period N] [--passed id1,id2,...]\n"
           << "       [--init --seed N --attempts N --repair-steps N --rcl N"
              " --eval-budget N] [--advance-passed id1,id2,...]\n"
           << "       [--simulate --approval-prob Q --simulation-seed N"
              " --course-approval id=Q ... --period-cap N]\n"
           << "       [--mmas --init-budget N --ants N --alpha A --beta B"
              " --evaporation R --tau-min-ratio R] [--reference-value N]"
              " [--results-dir RUTA]\n"
           << "Lee la instancia, prepara X_t y genera planes iniciales o ejecuta MMAS.\n";
    output << "--mmas usa --eval-budget como presupuesto global y --init-budget"
              " como reserva inicial.\n";
    output << "--advance-passed requiere --init o --mmas;"
              " use una cadena vacía si todos reprueban.\n";
    output << "--simulate requiere --init o --mmas, Q en [0,1] y una semilla;"
              " replanifica hasta T o la cota indicada.\n";
    output << "--period-cap requiere --mmas y --simulate o --advance-passed;"
              " permite replanificar hasta N > T.\n";
    output << "Toda ejecución de --init o --mmas guarda un .md detallado y "
              "una fila en resultados.csv dentro de results/ (o --results-dir).\n";
    output << "--reference-value N calcula el gap solo para una planificación"
              " estática con referencia validada.\n";
}

int parse_period(const std::string& text, const std::string& option) {
    int result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        result < 1) {
        throw std::invalid_argument(option + " requiere un entero positivo");
    }
    return result;
}

std::uint64_t parse_unsigned(const std::string& text, const std::string& option) {
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (text.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != text.data() + text.size()) {
        throw std::invalid_argument(option + " requiere un entero no negativo");
    }
    return result;
}

double parse_probability(const std::string& text, const std::string& option) {
    std::size_t used = 0;
    double result = 0.0;
    try {
        result = std::stod(text, &used);
    } catch (const std::invalid_argument&) {
        throw std::invalid_argument(option + " requiere una probabilidad en [0,1]");
    } catch (const std::out_of_range&) {
        throw std::invalid_argument(option + " requiere una probabilidad en [0,1]");
    }
    if (used != text.size() || !std::isfinite(result) ||
        result < 0.0 || result > 1.0) {
        throw std::invalid_argument(option + " requiere una probabilidad en [0,1]");
    }
    return result;
}

double parse_nonnegative_real(const std::string& text,
                              const std::string& option) {
    std::size_t used = 0;
    double result = 0.0;
    try {
        result = std::stod(text, &used);
    } catch (const std::invalid_argument&) {
        throw std::invalid_argument(option + " requiere un número no negativo");
    } catch (const std::out_of_range&) {
        throw std::invalid_argument(option + " requiere un número no negativo");
    }
    if (used != text.size() || !std::isfinite(result) || result < 0.0) {
        throw std::invalid_argument(option + " requiere un número no negativo");
    }
    return result;
}

void append_passed_ids(const std::string& text, std::vector<std::string>& passed) {
    std::size_t start = 0;
    do {
        const std::size_t comma = text.find(',', start);
        const std::string id = text.substr(start, comma - start);
        if (id.empty()) {
            throw std::invalid_argument("--passed requiere identificadores separados por coma");
        }
        passed.push_back(id);
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    } while (true);
}

void print_ids(const std::vector<std::string>& ids) {
    if (ids.empty()) {
        std::cout << " ninguno";
    } else {
        for (const std::string& id : ids) {
            std::cout << ' ' << id;
        }
    }
    std::cout << '\n';
}

void print_plan(const dbacp::Instance& instance, const dbacp::AcademicState& state,
                const dbacp::InitializedSolution& best,
                const std::string& label) {
    std::cout << label << ": carga máxima="
              << best.evaluation.maximum_load << ", E="
              << best.evaluation.squared_balance_error << '\n';
    for (int period = state.current_period; period <= instance.periods; ++period) {
        const std::size_t offset =
            static_cast<std::size_t>(period - state.current_period);
        std::cout << "  Semestre " << period << " ("
                  << best.evaluation.credits_per_period[offset]
                  << " créditos, "
                  << best.evaluation.courses_per_period[offset]
                  << " cursos):";
        for (std::size_t k = 0; k < best.plan.course_indices.size(); ++k) {
            if (best.plan.assigned_periods[k] == period) {
                std::cout << ' '
                          << instance.courses[best.plan.course_indices[k]].id;
            }
        }
        std::cout << '\n';
    }
}

void print_infeasible(const dbacp::InitializedSolution& solution) {
    const auto& evaluation = solution.evaluation;
    std::cout << "Mejor intento infactible: V_L="
              << evaluation.credit_violation << ", V_N="
              << evaluation.course_violation << '\n';
    for (const std::string& violation : evaluation.violations) {
        std::cout << "  Restricción: " << violation << '\n';
    }
}

std::optional<dbacp::InitializedSolution> show_initialization(
    const dbacp::Instance& instance, const dbacp::AcademicState& state,
    const dbacp::InitializationOptions& options, dbacp::RunReport& report) {
    const auto started = std::chrono::steady_clock::now();
    const dbacp::InitializationResult result =
        dbacp::initialize_plans(instance, state, options);
    const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    std::cout << "Inicialización t=" << state.current_period << ": "
              << result.attempts_used << " intentos, " << result.evaluations
              << " evaluaciones, " << result.feasible_solutions.size()
              << " planes factibles distintos\n";
    for (const std::string& diagnostic : result.diagnostics) {
        std::cout << "Diagnóstico: " << diagnostic << '\n';
    }
    if (result.feasible_solutions.empty()) {
        std::cout << "No hay un plan factible validado para este estado.\n";
        if (result.best_infeasible) {
            print_infeasible(*result.best_infeasible);
        }
        report.record_search(instance, state, "original", std::nullopt,
                             result.best_infeasible, result.diagnostics,
                             result.evaluations, result.evaluations, 0, 0, seconds);
        return std::nullopt;
    }

    const dbacp::InitializedSolution* best = &result.feasible_solutions.front();
    for (const auto& candidate : result.feasible_solutions) {
        if (dbacp::better_plan(candidate.evaluation, best->evaluation)) {
            best = &candidate;
        }
    }
    print_plan(instance, state, *best, "Mejor plan factible");
    report.record_search(instance, state, "original", *best,
                         result.best_infeasible, result.diagnostics,
                         result.evaluations, result.evaluations, 0, 0, seconds);
    return *best;
}

std::optional<dbacp::InitializedSolution> show_mmas(
    const dbacp::Instance& instance, const dbacp::AcademicState& state,
    const dbacp::MmasOptions& options, dbacp::RunReport& report,
    const std::string& phase) {
    const auto started = std::chrono::steady_clock::now();
    const dbacp::MmasResult result = dbacp::run_mmas(instance, state, options);
    const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    std::cout << "MMAS t=" << state.current_period << ": "
              << result.evaluations << " evaluaciones ("
              << result.initialization_evaluations << " inicialización), "
              << result.ants_built << " hormigas, "
              << result.iterations << " iteraciones, "
              << result.improvements << " mejoras\n";
    for (const std::string& diagnostic : result.diagnostics) {
        std::cout << "Diagnóstico: " << diagnostic << '\n';
    }
    if (!result.best_feasible) {
        std::cout << "No hay un plan factible validado para este estado.\n";
        if (result.best_infeasible) {
            print_infeasible(*result.best_infeasible);
        }
        report.record_search(instance, state, phase, std::nullopt,
                             result.best_infeasible, result.diagnostics,
                             result.evaluations, result.initialization_evaluations,
                             result.ants_built, result.iterations, seconds);
        return std::nullopt;
    }
    print_plan(instance, state, *result.best_feasible, "Mejor plan MMAS");
    report.record_search(instance, state, phase, result.best_feasible,
                         result.best_infeasible, result.diagnostics,
                         result.evaluations, result.initialization_evaluations,
                         result.ants_built, result.iterations, seconds);
    return result.best_feasible;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        usage(argv[0], std::cout);
        return 0;
    }

    if (argc < 2) {
        usage(argv[0], std::cerr);
        return 2;
    }

    std::unique_ptr<dbacp::RunReport> report;
    const auto finish_report = [&](int status, const std::string& reason) {
        if (report) {
            report->finish(status, reason);
            std::cout << "Reporte guardado: " << report->path() << '\n';
        }
        return status;
    };
    try {
        int current_period = 1;
        bool period_given = false;
        bool initialize = false;
        bool mmas = false;
        bool simulate = false;
        std::vector<std::string> passed_ids;
        std::optional<std::uint64_t> seed;
        std::optional<std::uint64_t> attempts;
        std::optional<std::uint64_t> repair_steps;
        std::optional<std::uint64_t> rcl;
        std::optional<std::uint64_t> evaluation_budget;
        std::optional<std::uint64_t> init_budget;
        std::optional<std::uint64_t> ants;
        std::optional<double> alpha;
        std::optional<double> beta;
        std::optional<double> evaporation;
        std::optional<double> tau_min_ratio;
        std::optional<std::vector<std::string>> advance_passed;
        std::optional<double> approval_probability;
        std::optional<std::uint64_t> simulation_seed;
        std::optional<int> period_cap;
        std::optional<int> reference_value;
        std::string results_dir = "results";
        bool results_dir_given = false;
        std::vector<std::pair<std::string, double>> course_probabilities;
        for (int i = 2; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--period" && i + 1 < argc && !period_given) {
                current_period = parse_period(argv[++i], option);
                period_given = true;
            } else if (option == "--passed" && i + 1 < argc) {
                append_passed_ids(argv[++i], passed_ids);
            } else if (option == "--init" && !initialize) {
                initialize = true;
            } else if (option == "--mmas" && !mmas) {
                mmas = true;
            } else if (option == "--seed" && i + 1 < argc && !seed) {
                seed = parse_unsigned(argv[++i], option);
            } else if (option == "--attempts" && i + 1 < argc && !attempts) {
                attempts = parse_unsigned(argv[++i], option);
            } else if (option == "--repair-steps" && i + 1 < argc && !repair_steps) {
                repair_steps = parse_unsigned(argv[++i], option);
            } else if (option == "--rcl" && i + 1 < argc && !rcl) {
                rcl = parse_unsigned(argv[++i], option);
            } else if (option == "--eval-budget" && i + 1 < argc && !evaluation_budget) {
                evaluation_budget = parse_unsigned(argv[++i], option);
            } else if (option == "--init-budget" && i + 1 < argc && !init_budget) {
                init_budget = parse_unsigned(argv[++i], option);
            } else if (option == "--ants" && i + 1 < argc && !ants) {
                ants = parse_unsigned(argv[++i], option);
            } else if (option == "--alpha" && i + 1 < argc && !alpha) {
                alpha = parse_nonnegative_real(argv[++i], option);
            } else if (option == "--beta" && i + 1 < argc && !beta) {
                beta = parse_nonnegative_real(argv[++i], option);
            } else if (option == "--evaporation" && i + 1 < argc &&
                       !evaporation) {
                evaporation = parse_probability(argv[++i], option);
            } else if (option == "--tau-min-ratio" && i + 1 < argc &&
                       !tau_min_ratio) {
                tau_min_ratio = parse_probability(argv[++i], option);
            } else if (option == "--advance-passed" && i + 1 < argc &&
                       !advance_passed) {
                advance_passed.emplace();
                const std::string value = argv[++i];
                if (!value.empty()) {
                    append_passed_ids(value, *advance_passed);
                }
            } else if (option == "--simulate" && !simulate) {
                simulate = true;
            } else if (option == "--approval-prob" && i + 1 < argc &&
                       !approval_probability) {
                approval_probability = parse_probability(argv[++i], option);
            } else if (option == "--simulation-seed" && i + 1 < argc &&
                       !simulation_seed) {
                simulation_seed = parse_unsigned(argv[++i], option);
            } else if (option == "--period-cap" && i + 1 < argc &&
                       !period_cap) {
                period_cap = parse_period(argv[++i], option);
            } else if (option == "--reference-value" && i + 1 < argc &&
                       !reference_value) {
                reference_value = parse_period(argv[++i], option);
            } else if (option == "--results-dir" && i + 1 < argc &&
                       !results_dir_given) {
                results_dir = argv[++i];
                results_dir_given = true;
                if (results_dir.empty()) {
                    throw std::invalid_argument("--results-dir requiere una ruta no vacía");
                }
            } else if (option == "--course-approval" && i + 1 < argc) {
                const std::string value = argv[++i];
                const std::size_t separator = value.find('=');
                if (separator == std::string::npos || separator == 0 ||
                    separator + 1 == value.size()) {
                    throw std::invalid_argument(
                        "--course-approval requiere id=probabilidad");
                }
                course_probabilities.emplace_back(
                    value.substr(0, separator),
                    parse_probability(value.substr(separator + 1), option));
            } else {
                throw std::invalid_argument("opción inválida o incompleta: " + option);
            }
        }
        const bool has_initialization_parameters =
            seed || attempts || repair_steps || rcl || evaluation_budget;
        const bool solve = initialize || mmas;
        if (solve != has_initialization_parameters ||
            (solve && (!seed || !attempts || !repair_steps || !rcl ||
                       !evaluation_budget))) {
            throw std::invalid_argument(
                "--init/--mmas requiere --seed, --attempts, --repair-steps, "
                "--rcl y --eval-budget");
        }
        const bool has_mmas_parameters = init_budget || ants || alpha || beta ||
                                         evaporation || tau_min_ratio;
        if (mmas && (!init_budget || !ants || !alpha || !beta ||
                     !evaporation || !tau_min_ratio)) {
            throw std::invalid_argument(
                "--mmas requiere --init-budget, --ants, --alpha, --beta, "
                "--evaporation y --tau-min-ratio");
        }
        if (!mmas && has_mmas_parameters) {
            throw std::invalid_argument("los parámetros MMAS requieren --mmas");
        }
        if (advance_passed && !solve) {
            throw std::invalid_argument("--advance-passed requiere --init o --mmas");
        }
        if (simulate && (!solve || !approval_probability ||
                         !simulation_seed || advance_passed)) {
            throw std::invalid_argument(
                "--simulate requiere --init o --mmas, --approval-prob y "
                "--simulation-seed; "
                "no se combina con --advance-passed");
        }
        if (!simulate && (approval_probability || simulation_seed ||
                          !course_probabilities.empty())) {
            throw std::invalid_argument(
                "los parámetros de aprobación requieren --simulate");
        }
        if (period_cap && (!mmas || (!simulate && !advance_passed))) {
            throw std::invalid_argument(
                "--period-cap requiere --mmas con --simulate o --advance-passed");
        }
        if (results_dir_given && !solve) {
            throw std::invalid_argument("--results-dir requiere --init o --mmas");
        }
        if (reference_value && (!solve || simulate || advance_passed)) {
            throw std::invalid_argument(
                "--reference-value requiere una planificación estática");
        }

        const dbacp::Instance instance = dbacp::load_instance(argv[1]);
        std::optional<dbacp::Instance> extended_instance;
        if (period_cap) {
            if (*period_cap <= instance.periods ||
                *period_cap == std::numeric_limits<int>::max()) {
                throw std::invalid_argument("--period-cap debe ser mayor que T");
            }
            extended_instance = instance;
            extended_instance->periods = *period_cap;
            // La fase adicional admite pocos cursos pendientes (o períodos
            // vacíos), conservando máximos y prerrequisitos.
            extended_instance->min_credits = 0;
            extended_instance->min_courses = 0;
        }
        bool extension_active = period_cap && current_period > instance.periods;
        const dbacp::Instance* active_instance =
            extension_active ? &*extended_instance : &instance;
        std::vector<double> probabilities;
        if (simulate) {
            probabilities.assign(instance.courses.size(), *approval_probability);
            std::unordered_map<std::string, std::size_t> course_index;
            for (std::size_t i = 0; i < instance.courses.size(); ++i) {
                course_index.emplace(instance.courses[i].id, i);
            }
            std::unordered_set<std::string> overridden;
            for (const auto& override : course_probabilities) {
                const auto found = course_index.find(override.first);
                if (found == course_index.end()) {
                    throw std::invalid_argument(
                        "curso desconocido en --course-approval: " + override.first);
                }
                if (!overridden.insert(override.first).second) {
                    throw std::invalid_argument(
                        "probabilidad repetida para curso: " + override.first);
                }
                probabilities[found->second] = override.second;
            }
        }
        const dbacp::AcademicState state =
            dbacp::make_state(*active_instance, current_period, passed_ids);
        const dbacp::Plan plan = dbacp::make_empty_plan(*active_instance, state);
        std::cout << "Instancia leída: " << argv[1] << '\n'
                  << "Períodos: " << instance.periods << '\n'
                  << "Cursos: " << instance.courses.size() << '\n'
                  << "Créditos totales: " << instance.total_credits() << '\n'
                  << "Créditos por período: [" << instance.min_credits << ", "
                  << instance.max_credits << "]\n"
                  << "Cursos por período: [" << instance.min_courses << ", "
                  << instance.max_courses << "]\n"
                  << "Prerrequisitos únicos: " << instance.prerequisites.size() << '\n'
                  << "Validación estructural: correcta\n"
                  << "Estado: t=" << state.current_period
                  << ", aprobados=" << passed_ids.size()
                  << ", pendientes=" << plan.course_indices.size() << '\n'
                  << "Representación X_t: " << plan.assigned_periods.size()
                  << " posiciones; 0 indica aún sin asignar\n";
        if (period_cap) {
            std::cout << "Cota opcional de períodos: " << *period_cap << '\n';
        }
        if (!plan.course_indices.empty()) {
            std::cout << "Primeras posiciones: ";
            const std::size_t shown = std::min<std::size_t>(5, plan.course_indices.size());
            for (std::size_t k = 0; k < shown; ++k) {
                if (k > 0) {
                    std::cout << ", ";
                }
                std::cout << instance.courses[plan.course_indices[k]].id << "=?";
            }
            if (shown < plan.course_indices.size()) {
                std::cout << ", ...";
            }
            std::cout << '\n';
        }
        std::cout << "Plantilla: aún no es un plan completo\n";
        std::cout.flush();
        for (const std::string& warning : instance.warnings) {
            std::cerr << "Aviso: " << warning << '\n';
        }

        if (solve) {
            const dbacp::InitializationOptions options{
                *seed, static_cast<std::size_t>(*attempts),
                static_cast<std::size_t>(*repair_steps),
                static_cast<std::size_t>(*rcl),
                static_cast<std::size_t>(mmas ? *init_budget : *evaluation_budget)};
            std::optional<dbacp::MmasOptions> search_options;
            if (mmas) {
                search_options = dbacp::MmasOptions{
                    options, static_cast<std::size_t>(*ants), *alpha, *beta,
                    *evaporation, *tau_min_ratio,
                    static_cast<std::size_t>(*evaluation_budget)};
            }
            dbacp::ReportOptions report_options;
            report_options.directory = results_dir;
            report_options.instance_path = argv[1];
            report_options.algorithm = mmas ? "MMAS" : "inicializacion";
            report_options.seed = *seed;
            report_options.initialization = options;
            report_options.mmas = search_options;
            report_options.period_cap = period_cap;
            report_options.reference_value = reference_value;
            report_options.simulate = simulate;
            report_options.manual_advance = advance_passed.has_value();
            report_options.simulation_seed = simulation_seed;
            report_options.approval_probabilities = probabilities;
            report = std::make_unique<dbacp::RunReport>(instance, state,
                                                        report_options);
            const auto activate_extension = [&]() {
                extension_active = true;
                active_instance = &*extended_instance;
                std::cout << "Extensión activada: T=" << instance.periods
                          << ", cota=" << *period_cap
                          << "; mínimos de créditos y cursos=0 en la fase adicional.\n";
            };
            const auto solve_state = [&](const dbacp::AcademicState& academic_state)
                -> std::optional<dbacp::InitializedSolution> {
                if (period_cap && !extension_active &&
                    academic_state.current_period > instance.periods) {
                    activate_extension();
                }
                if (extension_active) {
                    return show_mmas(*active_instance, academic_state,
                                     *search_options, *report, "extendida");
                }
                if (search_options) {
                    const auto found = show_mmas(instance, academic_state,
                                                 *search_options, *report, "original");
                    if (found || !period_cap) {
                        return found;
                    }
                    // También se activa si una reprobación deja sin plan
                    // validado antes de T; no se afirma infactibilidad exacta.
                    activate_extension();
                    return show_mmas(*active_instance, academic_state,
                                     *search_options, *report, "extendida");
                }
                return show_initialization(instance, academic_state, options,
                                           *report);
            };
            if (extension_active) {
                activate_extension();
            }
            const auto best = solve_state(state);
            if (!best) {
                return finish_report(3, "no se encontró plan factible validado");
            }
            if (advance_passed) {
                const dbacp::AcademicState next = dbacp::advance_state(
                    *active_instance, state, best->plan, *advance_passed);
                dbacp::SemesterOutcome outcome;
                outcome.passed_ids = *advance_passed;
                for (std::size_t k = 0; k < best->plan.course_indices.size(); ++k) {
                    if (best->plan.assigned_periods[k] != state.current_period) {
                        continue;
                    }
                    const std::string& id = instance.courses[
                        best->plan.course_indices[k]].id;
                    if (std::find(outcome.passed_ids.begin(),
                                  outcome.passed_ids.end(), id) ==
                        outcome.passed_ids.end()) {
                        outcome.failed_ids.push_back(id);
                    }
                }
                report->record_semester(*active_instance, state, best->plan,
                                        outcome, next, false);
                const std::size_t pending = static_cast<std::size_t>(
                    std::count(next.passed.begin(), next.passed.end(), 0));
                std::cout << "Tras semestre " << state.current_period
                          << ": aprobados ahora=" << advance_passed->size()
                          << ", pendientes=" << pending
                          << ", siguiente t=" << next.current_period << '\n';
                if (pending != 0 && next.current_period >
                                        (period_cap ? *period_cap : instance.periods)) {
                    std::cout << "Sin planificación factible: cota de períodos alcanzada.\n";
                    return finish_report(3, "cota de períodos alcanzada con cursos pendientes");
                }
                if (pending != 0 && !solve_state(next)) {
                    return finish_report(3, "no se encontró plan factible validado tras el avance");
                }
            }
            if (simulate) {
                std::cout << "Escenario Bernoulli: q general="
                          << *approval_probability << ", ajustes por curso="
                          << course_probabilities.size() << ", semilla="
                          << *simulation_seed << '\n';
                dbacp::AcademicState current = state;
                dbacp::InitializedSolution current_best = *best;
                while (current.current_period <= active_instance->periods) {
                    const dbacp::SemesterOutcome outcome =
                        dbacp::simulate_semester(*active_instance, current,
                                                 current_best.plan,
                                                 probabilities,
                                                 *simulation_seed);
                    std::cout << "Simulación semestre " << current.current_period
                              << " (semilla " << *simulation_seed << "):\n"
                              << "  Aprobadas:";
                    print_ids(outcome.passed_ids);
                    std::cout << "  Reprobadas:";
                    print_ids(outcome.failed_ids);

                    const dbacp::AcademicState next = dbacp::advance_state(
                        *active_instance, current, current_best.plan,
                        outcome.passed_ids);
                    report->record_semester(*active_instance, current,
                                            current_best.plan, outcome, next, true);
                    const std::size_t pending = static_cast<std::size_t>(
                        std::count(next.passed.begin(), next.passed.end(), 0));
                    std::cout << "Estado siguiente: t=" << next.current_period
                              << ", aprobados="
                              << instance.courses.size() - pending
                              << ", pendientes=" << pending << '\n';
                    if (pending == 0) {
                        std::cout << "Trayectoria completada en el semestre "
                                  << current.current_period << ".\n";
                        return finish_report(0, "trayectoria académica completada");
                    }
                    if (next.current_period >
                        (period_cap ? *period_cap : instance.periods)) {
                        std::cout << "Sin planificación factible: " << pending
                                  << " cursos pendientes después de "
                                  << (period_cap ? "la cota " : "T=")
                                  << (period_cap ? *period_cap : instance.periods)
                                  << ":";
                        std::vector<std::string> pending_ids;
                        for (std::size_t i = 0; i < next.passed.size(); ++i) {
                            if (!next.passed[i]) {
                                pending_ids.push_back(instance.courses[i].id);
                            }
                        }
                        print_ids(pending_ids);
                        return finish_report(3, "cota de períodos alcanzada con cursos pendientes");
                    }
                    current = next;
                    const auto replanned = solve_state(current);
                    if (!replanned) {
                        return finish_report(3, "no se encontró plan factible validado tras reprobación");
                    }
                    current_best = *replanned;
                }
            }
        }
        return finish_report(0, advance_passed ? "avance y replanificación completados" :
                               solve ? "plan factible encontrado" : "instancia leída");
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        try {
            return finish_report(1, error.what());
        } catch (const std::exception& report_error) {
            std::cerr << "Error al guardar el reporte: "
                      << report_error.what() << '\n';
            return 1;
        }
    }
}
