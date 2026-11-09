CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

TARGET  = simulador
HEADERS = NANDFlash.hpp IODriver.hpp FTL.hpp SimpleFS.hpp Workload.hpp Shell.hpp Metrics.hpp Experiment.hpp Report.hpp ReportPage.hpp
TESTS   = tests/test_nand tests/test_driver tests/test_ftl_direct tests/test_ftl_mapping tests/test_wear_leveling tests/test_fs tests/test_shell tests/test_metrics tests/test_report

all: $(TARGET)

$(TARGET): main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) main.cpp -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

# Compila e executa os testes de cada camada
test: $(TESTS)
	./tests/test_nand
	./tests/test_driver
	./tests/test_ftl_direct
	./tests/test_ftl_mapping
	./tests/test_wear_leveling
	./tests/test_fs
	./tests/test_shell
	./tests/test_metrics
	./tests/test_report

tests/test_nand: tests/test_nand.cpp NANDFlash.hpp
	$(CXX) $(CXXFLAGS) tests/test_nand.cpp -o tests/test_nand

tests/test_driver: tests/test_driver.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) tests/test_driver.cpp -o tests/test_driver

tests/test_ftl_direct: tests/test_ftl_direct.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) tests/test_ftl_direct.cpp -o tests/test_ftl_direct

tests/test_ftl_mapping: tests/test_ftl_mapping.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) tests/test_ftl_mapping.cpp -o tests/test_ftl_mapping

tests/test_wear_leveling: tests/test_wear_leveling.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) tests/test_wear_leveling.cpp -o tests/test_wear_leveling

tests/test_fs: tests/test_fs.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) tests/test_fs.cpp -o tests/test_fs

tests/test_shell: tests/test_shell.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) tests/test_shell.cpp -o tests/test_shell

tests/test_metrics: tests/test_metrics.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) tests/test_metrics.cpp -o tests/test_metrics

tests/test_report: tests/test_report.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) tests/test_report.cpp -o tests/test_report

clean:
	rm -f $(TARGET) $(TARGET).exe $(TESTS) tests/*.exe *.o *.bin *.csv nand_config.txt resultados.html

.PHONY: all run test clean
