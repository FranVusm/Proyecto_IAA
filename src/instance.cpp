#include "dbacp/instance.hpp"

#include <algorithm>
#include <cctype>
#include <climits>
#include <fstream>
#include <iterator>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace dbacp {
namespace {

struct Token {
    std::string text;
    int line;
};

struct RawInstance {
    int periods = 0;
    int min_credits = 0;
    int max_credits = 0;
    int min_courses = 0;
    int max_courses = 0;
    std::vector<std::string> names;
    std::vector<int> credits;
    // El formato .dat usa (curso dependiente, prerrequisito).
    std::vector<std::pair<std::string, std::string>> prerequisites;
    std::vector<std::string> warnings;
};

std::string remove_comments(const std::string& source,
                            std::vector<std::string>& warnings) {
    std::string output;
    output.reserve(source.size());
    int line = 1;

    for (std::size_t i = 0; i < source.size();) {
        if (source[i] == '%' ||
            (source[i] == '/' && i + 1 < source.size() && source[i + 1] == '/')) {
            while (i < source.size() && source[i] != '\n') {
                output += ' ';
                ++i;
            }
            continue;
        }

        if (source[i] == '/' && i + 1 < source.size() && source[i + 1] == '*') {
            const std::size_t close = source.find("*/", i + 2);
            if (close == std::string::npos) {
                warnings.push_back("comentario /* sin cierre en línea " +
                                   std::to_string(line) +
                                   "; se ignoró solo hasta fin de línea");
                while (i < source.size() && source[i] != '\n') {
                    output += ' ';
                    ++i;
                }
            } else {
                const std::size_t end = close + 2;
                while (i < end) {
                    if (source[i] == '\n') {
                        output += '\n';
                        ++line;
                    } else {
                        output += ' ';
                    }
                    ++i;
                }
            }
            continue;
        }

        output += source[i];
        if (source[i] == '\n') {
            ++line;
        }
        ++i;
    }
    return output;
}

std::vector<Token> tokenize(const std::string& source) {
    std::vector<Token> tokens;
    int line = 1;
    for (std::size_t i = 0; i < source.size();) {
        const unsigned char ch = static_cast<unsigned char>(source[i]);
        if (std::isspace(ch)) {
            if (source[i] == '\n') {
                ++line;
            }
            ++i;
            continue;
        }
        if (std::isalpha(ch) || ch == '_' || std::isdigit(ch)) {
            const std::size_t start = i;
            while (i < source.size()) {
                const unsigned char next = static_cast<unsigned char>(source[i]);
                if (!std::isalnum(next) && next != '_') {
                    break;
                }
                ++i;
            }
            tokens.push_back({source.substr(start, i - start), line});
            continue;
        }
        if (std::string("=;{}[],<>").find(source[i]) != std::string::npos) {
            tokens.push_back({source.substr(i, 1), line});
            ++i;
            continue;
        }
        throw std::runtime_error("carácter inesperado en línea " +
                                 std::to_string(line) + ": '" + source[i] + "'");
    }
    return tokens;
}

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    RawInstance parse() {
        RawInstance raw;
        std::unordered_set<std::string> seen;

        while (!at_end()) {
            const std::string field = identifier();
            if (!seen.insert(field).second) {
                fail("campo repetido: " + field);
            }
            expect("=");

            if (field == "p") {
                raw.periods = integer();
            } else if (field == "a") {
                raw.min_credits = integer();
            } else if (field == "b") {
                raw.max_credits = integer();
            } else if (field == "c") {
                raw.min_courses = integer();
            } else if (field == "d") {
                raw.max_courses = integer();
            } else if (field == "courses") {
                raw.names = course_list();
            } else if (field == "credit") {
                raw.credits = credit_list();
            } else if (field == "prereq") {
                raw.prerequisites = prerequisite_list(raw.warnings);
            } else {
                fail("campo desconocido: " + field);
            }
            expect(";");
        }

        for (const char* field : {"p", "a", "b", "c", "d", "courses",
                                  "credit", "prereq"}) {
            if (!seen.count(field)) {
                throw std::runtime_error(std::string("falta el campo obligatorio: ") + field);
            }
        }
        return raw;
    }

private:
    std::vector<Token> tokens_;
    std::size_t position_ = 0;

    bool at_end() const { return position_ == tokens_.size(); }

    const std::string& peek() const {
        if (at_end()) {
            fail("fin inesperado del archivo");
        }
        return tokens_[position_].text;
    }

    [[noreturn]] void fail(const std::string& message) const {
        const int line = at_end() ? (tokens_.empty() ? 1 : tokens_.back().line)
                                  : tokens_[position_].line;
        throw std::runtime_error("línea " + std::to_string(line) + ": " + message);
    }

    void expect(const std::string& symbol) {
        if (peek() != symbol) {
            fail("se esperaba '" + symbol + "' y se encontró '" + peek() + "'");
        }
        ++position_;
    }

    std::string identifier() {
        const std::string value = peek();
        const unsigned char first = static_cast<unsigned char>(value[0]);
        if (!std::isalpha(first) && first != '_') {
            fail("se esperaba un identificador");
        }
        ++position_;
        return value;
    }

    int integer() {
        const std::string value = peek();
        if (!std::all_of(value.begin(), value.end(), [](unsigned char ch) {
                return std::isdigit(ch) != 0;
            })) {
            fail("se esperaba un entero no negativo");
        }
        unsigned long long number = 0;
        try {
            number = std::stoull(value);
        } catch (const std::exception&) {
            fail("entero fuera de rango: " + value);
        }
        if (number > INT_MAX) {
            fail("entero fuera de rango: " + value);
        }
        ++position_;
        return static_cast<int>(number);
    }

    std::vector<std::string> course_list() {
        expect("{");
        std::vector<std::string> names;
        while (peek() != "}") {
            names.push_back(identifier());
            if (peek() == ",") {
                ++position_;
            } else if (peek() != "}") {
                fail("falta coma entre cursos");
            }
        }
        expect("}");
        return names;
    }

    std::vector<int> credit_list() {
        expect("[");
        std::vector<int> credits;
        while (peek() != "]") {
            credits.push_back(integer());
            if (peek() == ",") {
                ++position_;
            } else if (peek() != "]") {
                fail("falta coma entre créditos");
            }
        }
        expect("]");
        return credits;
    }

    std::vector<std::pair<std::string, std::string>> prerequisite_list(
        std::vector<std::string>& warnings) {
        expect("{");
        std::vector<std::pair<std::string, std::string>> pairs;
        while (peek() != "}") {
            expect("<");
            const std::string dependent = identifier();
            expect(",");
            const std::string prerequisite = identifier();
            expect(">");
            pairs.emplace_back(dependent, prerequisite);

            if (peek() == ",") {
                ++position_;
            } else if (peek() == "<") {
                warnings.push_back("falta coma entre prerrequisitos cerca de línea " +
                                   std::to_string(tokens_[position_].line) +
                                   "; se interpretaron como pares separados");
            } else if (peek() != "}") {
                fail("se esperaba coma o cierre después de un prerrequisito");
            }
        }
        expect("}");
        return pairs;
    }
};

void validate_bounds(const Instance& instance) {
    if (instance.periods <= 0) {
        throw std::runtime_error("p debe ser mayor que cero");
    }
    if (instance.min_credits > instance.max_credits ||
        instance.min_courses > instance.max_courses) {
        throw std::runtime_error("los límites mínimos superan los máximos");
    }
    const std::int64_t periods = instance.periods;
    const std::int64_t count = static_cast<std::int64_t>(instance.courses.size());
    const std::int64_t credits = instance.total_credits();
    if (count < periods * instance.min_courses ||
        count > periods * instance.max_courses) {
        throw std::runtime_error("cantidad total de cursos incompatible con los límites por período");
    }
    if (credits < periods * instance.min_credits ||
        credits > periods * instance.max_credits) {
        throw std::runtime_error("créditos totales incompatibles con los límites por período");
    }
}

void validate_prerequisite_graph(const Instance& instance) {
    std::vector<std::vector<std::size_t>> successors(instance.courses.size());
    std::vector<int> indegree(instance.courses.size(), 0);
    std::vector<int> longest_path(instance.courses.size(), 1);

    for (const auto& edge : instance.prerequisites) {
        successors[edge.first].push_back(edge.second);
        ++indegree[edge.second];
    }

    std::queue<std::size_t> ready;
    for (std::size_t i = 0; i < indegree.size(); ++i) {
        if (indegree[i] == 0) {
            ready.push(i);
        }
    }

    std::size_t visited = 0;
    while (!ready.empty()) {
        const std::size_t current = ready.front();
        ready.pop();
        ++visited;
        for (const std::size_t next : successors[current]) {
            longest_path[next] = std::max(longest_path[next],
                                          longest_path[current] + 1);
            if (--indegree[next] == 0) {
                ready.push(next);
            }
        }
    }

    if (visited != instance.courses.size()) {
        throw std::runtime_error("los prerrequisitos contienen un ciclo");
    }
    if (*std::max_element(longest_path.begin(), longest_path.end()) >
        instance.periods) {
        throw std::runtime_error("una cadena de prerrequisitos excede los períodos disponibles");
    }
}

}  // namespace

std::int64_t Instance::total_credits() const {
    std::int64_t total = 0;
    for (const Course& course : courses) {
        total += course.credits;
    }
    return total;
}

Instance load_instance(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("no se pudo abrir '" + path + "'");
    }
    const std::string source(std::istreambuf_iterator<char>{input},
                             std::istreambuf_iterator<char>{});

    std::vector<std::string> warnings;
    const std::string clean = remove_comments(source, warnings);
    RawInstance raw = Parser(tokenize(clean)).parse();
    warnings.insert(warnings.end(), raw.warnings.begin(), raw.warnings.end());

    if (raw.names.empty()) {
        throw std::runtime_error("la lista de cursos está vacía");
    }
    if (raw.names.size() != raw.credits.size()) {
        throw std::runtime_error("hay " + std::to_string(raw.names.size()) +
                                 " cursos pero " + std::to_string(raw.credits.size()) +
                                 " valores de crédito");
    }

    Instance instance;
    instance.periods = raw.periods;
    instance.min_credits = raw.min_credits;
    instance.max_credits = raw.max_credits;
    instance.min_courses = raw.min_courses;
    instance.max_courses = raw.max_courses;
    instance.warnings = std::move(warnings);

    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < raw.names.size(); ++i) {
        if (!index.emplace(raw.names[i], i).second) {
            throw std::runtime_error("curso repetido: " + raw.names[i]);
        }
        if (raw.credits[i] <= 0) {
            throw std::runtime_error("el curso " + raw.names[i] +
                                     " debe tener créditos positivos");
        }
        if (raw.credits[i] > instance.max_credits) {
            throw std::runtime_error("el curso " + raw.names[i] +
                                     " supera el máximo de créditos por período");
        }
        instance.courses.push_back({raw.names[i], raw.credits[i]});
    }

    std::set<std::pair<std::size_t, std::size_t>> unique_edges;
    for (const auto& pair : raw.prerequisites) {
        const auto dependent = index.find(pair.first);
        const auto prerequisite = index.find(pair.second);
        if (dependent == index.end() || prerequisite == index.end()) {
            throw std::runtime_error("prerrequisito con curso desconocido: <" +
                                     pair.first + "," + pair.second + ">");
        }
        if (dependent->second == prerequisite->second) {
            throw std::runtime_error("un curso no puede ser prerrequisito de sí mismo: " +
                                     pair.first);
        }
        const auto edge = std::make_pair(prerequisite->second, dependent->second);
        if (unique_edges.insert(edge).second) {
            instance.prerequisites.push_back(edge);
        } else {
            instance.warnings.push_back("prerrequisito repetido <" + pair.first +
                                        "," + pair.second + ">; se conservó una vez");
        }
    }

    validate_bounds(instance);
    validate_prerequisite_graph(instance);
    return instance;
}

}  // namespace dbacp
