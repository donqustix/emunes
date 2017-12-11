CXX = clang++-5.0
CXXFLAGS = -std=c++1z -pthread -pedantic -Wall -Wextra -DNDEBUG -march=native -mtune=native -O2 -Isrc
LDFLAGS = -lSDL2 -lSDL2_net

PROJECT_NAME = emunes
PROJECT_SRCS = $(wildcard src/*.cpp) $(wildcard src/*/*/*.cpp)

all: $(PROJECT_SRCS)
	$(CXX) $(CXXFLAGS) $^ -o bin/$(PROJECT_NAME) $(LDFLAGS)

run:
	bin/$(PROJECT_NAME)

bin/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

