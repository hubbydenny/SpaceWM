CXX ?= clang++
CXXFLAGS ?= -std=c++20 -Wall -Wextra
LDLIBS = $(shell pkg-config --libs wayland-client)

spacewc: src/main.cpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o $@ $(LDLIBS)

clean:
	rm -f spacewc

.PHONY: clean
