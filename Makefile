CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

all: umbra umbra-server

umbra: client.cpp config.h
	$(CXX) $(CXXFLAGS) main.cpp -o umbra

umbra-server: server.cpp
	$(CXX) $(CXXFLAGS) server.cpp -o umbra-server

clean:
	rm -f umbra umbra-server

.PHONY: all clean
