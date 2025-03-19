CXX = g++
CXXFLAGS = -Wall -Wextra -pthread -O2 -fPIC -std=c++17
INCLUDES = -Iinclude/private -Iinclude/comapi -I/usr/include
LDFLAGS = -lpthread  -lboost_filesystem -lboost_iostreams -lboost_system

PREFIX ?= /usr/local
BINARYNAME = containerInitd

SRCS = $(wildcard src/*.cpp)
OBJS = $(SRCS:.cpp=.o)

all: $(BINARYNAME)

$(BINARYNAME): $(OBJS)
	@echo "Creating binary $@"
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(LDFLAGS)

src/%.o: src/%.cpp
	@echo "Compiling $< into $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@echo "Cleaning up"
	rm -f $(OBJS) $(BINARYNAME)

.PHONY: all clean
