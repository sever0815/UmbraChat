CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

all: umbra umbra-server

umbra: main.cpp config.h
        $(CXX) $(CXXFLAGS) main.cpp -o cchat

umbra-server: server.cpp
        $(CXX) $(CXXFLAGS) server.cpp -o cchat-server

clean:
        rm -f cchat cchat-server

.PHONY: all clean
