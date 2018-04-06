CXX		= g++
CXXFLAG	= -g -std=c++11 -Wno-write-strings
INCL	= -I ../../src
TESTS	= test test2
SOURCE 	= $(wildcard ../../src/*.cpp)

.PHONY: all clean

all: $(TESTS)

clean:
	rm -f $(TESTS)

test: test.cpp $(SOURCE)
	$(CXX) $(CXXFLAG) $(INCL) -o $@ $^

test2: test2.cpp $(SOURCE)
	$(CXX) $(CXXFLAG) $(INCL) -o $@ $^
