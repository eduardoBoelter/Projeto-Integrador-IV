CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

TARGET = simulador

all: $(TARGET)

$(TARGET): main.cpp NANDFlash.hpp
	$(CXX) $(CXXFLAGS) main.cpp -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe *.o nand_device.bin

.PHONY: all run clean
