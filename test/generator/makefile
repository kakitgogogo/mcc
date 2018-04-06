CXX		= g++
CXXFLAG	= -g -std=c++11 -Wno-write-strings -DDEBUG_MODE
INCL	= -I ../../src
TESTS	= test gen
SOURCE 	= $(wildcard ../../src/*.cpp)

.PHONY: all clean

all: $(TESTS)

clean:
	rm -f $(TESTS)

test: test.cpp $(SOURCE)
	$(CXX) $(CXXFLAG) $(INCL) -o $@ $^

gen: gen.cpp $(SOURCE)
	$(CXX) $(CXXFLAG) $(INCL) -o $@ $^