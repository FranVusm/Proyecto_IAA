CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude
TARGET := min_max
SOURCES := src/main.cpp src/instance.cpp src/plan.cpp src/initialization.cpp src/simulation.cpp src/mmas.cpp src/report.cpp

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SOURCES) include/dbacp/instance.hpp include/dbacp/plan.hpp include/dbacp/initialization.hpp include/dbacp/simulation.hpp include/dbacp/mmas.hpp include/dbacp/report.hpp
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

build/reader_test: tests/reader_test.cpp src/instance.cpp include/dbacp/instance.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) tests/reader_test.cpp src/instance.cpp -o $@

build/plan_test: tests/plan_test.cpp src/instance.cpp src/plan.cpp include/dbacp/plan.hpp include/dbacp/instance.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) tests/plan_test.cpp src/instance.cpp src/plan.cpp -o $@

build/initialization_test: tests/initialization_test.cpp src/instance.cpp src/plan.cpp src/initialization.cpp include/dbacp/initialization.hpp include/dbacp/plan.hpp include/dbacp/instance.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) tests/initialization_test.cpp src/instance.cpp src/plan.cpp src/initialization.cpp -o $@

build/simulation_test: tests/simulation_test.cpp src/instance.cpp src/plan.cpp src/simulation.cpp include/dbacp/simulation.hpp include/dbacp/plan.hpp include/dbacp/instance.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) tests/simulation_test.cpp src/instance.cpp src/plan.cpp src/simulation.cpp -o $@

build/mmas_test: tests/mmas_test.cpp src/instance.cpp src/plan.cpp src/initialization.cpp src/mmas.cpp include/dbacp/mmas.hpp include/dbacp/initialization.hpp include/dbacp/plan.hpp include/dbacp/instance.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) tests/mmas_test.cpp src/instance.cpp src/plan.cpp src/initialization.cpp src/mmas.cpp -o $@

test: $(TARGET) build/reader_test build/plan_test build/initialization_test build/simulation_test build/mmas_test
	./build/reader_test
	./build/plan_test
	./build/initialization_test
	./build/simulation_test
	./build/mmas_test
	bash tests/period_cap_test.sh

clean:
	rm -f $(TARGET) build/reader_test build/plan_test build/initialization_test build/simulation_test build/mmas_test
