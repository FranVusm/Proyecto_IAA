#include "dbacp/instance.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error("prueba fallida: " + message);
    }
}

std::size_t course_index(const dbacp::Instance& instance, const std::string& id) {
    const auto found = std::find_if(instance.courses.begin(), instance.courses.end(),
                                    [&](const dbacp::Course& course) {
                                        return course.id == id;
                                    });
    check(found != instance.courses.end(), "curso " + id + " no encontrado");
    return static_cast<std::size_t>(found - instance.courses.begin());
}

void expect_error(const std::string& contents, const std::string& expected) {
    const std::string path = "build/reader_invalid.dat";
    {
        std::ofstream output(path);
        check(static_cast<bool>(output), "no se pudo crear instancia temporal");
        output << contents;
    }

    bool matched = false;
    try {
        dbacp::load_instance(path);
    } catch (const std::runtime_error& error) {
        matched = std::string(error.what()).find(expected) != std::string::npos;
    }
    std::remove(path.c_str());
    check(matched, "se esperaba rechazo por " + expected);
}

}  // namespace

int main() {
    try {
        const dbacp::Instance bacp8 =
            dbacp::load_instance("instances/classic/bacp8.dat");
        const dbacp::Instance bacp10 =
            dbacp::load_instance("instances/classic/bacp10.dat");
        const dbacp::Instance bacp12 =
            dbacp::load_instance("instances/classic/bacp12.dat");

        check(bacp8.periods == 8 && bacp8.courses.size() == 46 &&
                  bacp8.total_credits() == 133 && bacp8.prerequisites.size() == 33,
              "conteos BACP8");
        check(bacp10.periods == 10 && bacp10.courses.size() == 42 &&
                  bacp10.total_credits() == 134 && bacp10.prerequisites.size() == 34,
              "conteos BACP10");
        check(bacp12.periods == 12 && bacp12.courses.size() == 66 &&
                  bacp12.total_credits() == 204 && bacp12.prerequisites.size() == 65,
              "conteos BACP12");

        const auto edge = std::make_pair(course_index(bacp8, "dew100"),
                                         course_index(bacp8, "dew101"));
        check(std::find(bacp8.prerequisites.begin(), bacp8.prerequisites.end(), edge) !=
                  bacp8.prerequisites.end(),
              "orientación prerrequisito -> curso dependiente");
        check(bacp8.warnings.size() == 5 && bacp10.warnings.size() == 1 &&
                  bacp12.warnings.size() == 4,
              "avisos de formato de las instancias originales");

        expect_error("p=2; a=0; b=4; c=0; d=2; courses={x,y}; "
                     "credit=[1]; prereq={};",
                     "valores de crédito");
        expect_error("p=2; a=0; b=4; c=0; d=2; courses={x,y}; "
                     "credit=[1,1]; prereq={<x,z>};",
                     "curso desconocido");
        expect_error("p=2; a=0; b=4; c=0; d=2; courses={x,y}; "
                     "credit=[1,1]; prereq={<x,y>,<y,x>};",
                     "ciclo");

        std::cout << "Lector: 3 instancias válidas y 3 casos inválidos verificados.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
