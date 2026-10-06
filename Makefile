CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

TARGET  = simulador
HEADERS = NANDFlash.hpp IODriver.hpp
TESTS   = tests/test_nand tests/test_driver

all: $(TARGET)

$(TARGET): main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) main.cpp -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

# Compila e executa os testes de cada camada
test: $(TESTS)
	./tests/test_nand
	./tests/test_driver

tests/test_nand: tests/test_nand.cpp NANDFlash.hpp
	$(CXX) $(CXXFLAGS) tests/test_nand.cpp -o tests/test_nand

tests/test_driver: tests/test_driver.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) tests/test_driver.cpp -o tests/test_driver

clean:
	rm -f $(TARGET) $(TARGET).exe $(TESTS) tests/*.exe *.o *.bin

.PHONY: all run test clean
