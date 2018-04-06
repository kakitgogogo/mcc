CXX		= g++
CXXFLAG	= -g -std=c++11 -Wno-write-strings -DDEBUG_MODE
INCL	= -I ../../src
TESTS	= draw
SOURCE 	= $(wildcard ../../src/*.cpp)

.PHONY: all clean

all: $(TESTS)

clean:
	rm -f $(TESTS)

draw: draw.cpp $(SOURCE)
	$(CXX) $(CXXFLAG) $(INCL) -o $@ $^
