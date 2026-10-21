#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace dbacp {

struct Course {
    std::string id;
    int credits;
};

struct Instance {
    int periods = 0;
    int min_credits = 0;
    int max_credits = 0;
    int min_courses = 0;
    int max_courses = 0;
    std::vector<Course> courses;
    // Cada par contiene índices (prerrequisito, curso dependiente).
    std::vector<std::pair<std::size_t, std::size_t>> prerequisites;
    std::vector<std::string> warnings;

    std::int64_t total_credits() const;
};

// Lanza std::runtime_error cuando la instancia no se puede leer o validar.
Instance load_instance(const std::string& path);

}  // namespace dbacp
