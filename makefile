CXX		= g++
CXXFLAG	= -g -std=c++11 -Wno-write-strings -DDEBUG_MODE
INCL	= -I ./src
SOURCE 	= $(wildcard src/*.cpp)

.PHONY: all_test clean mcc

mcc: main.cpp $(SOURCE)
	export POSIXLY_CORRECT=y
	sudo cp include/*.h /usr/local/mcc/include
	$(CXX) $(CXXFLAG) $(INCL) -o $@ $^

all_test:
	cd test/misc && $(MAKE) all
	cd test/eval && $(MAKE) all
	cd test/lexer && $(MAKE) all
	cd test/preprocessor && $(MAKE) all
	cd test/parser && $(MAKE) all
	cd test/generator && $(MAKE) all
	cd tools && $(MAKE) all

clean:
	rm -f mcc
	cd test/misc && $(MAKE) clean
	cd test/eval && $(MAKE) clean
	cd test/lexer && $(MAKE) clean
	cd test/preprocessor && $(MAKE) clean
	cd test/parser && $(MAKE) clean
	cd test/generator && $(MAKE) clean
	cd tools && $(MAKE) clean
