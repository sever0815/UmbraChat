CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
TARGET = cchat

all: $(TARGET)

$(TARGET): main.cpp config.h
	$(CXX) $(CXXFLAGS) main.cpp -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
