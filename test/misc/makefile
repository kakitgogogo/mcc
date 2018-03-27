CC		= g++
CXXFLAG	= -g -std=c++11 -Wall -Wno-write-strings
INCL	= -I ../../src
TEST	= tbuffer terror tutils tencode tfile ttoken ttype tscope tast
SOURCE 	= $(wildcard ../../src/*.cpp)

.PHONY: all clean

all: $(TEST)

clean:
	rm -f $(TEST)

tbuffer: tbuffer.cpp $(SOURCE)
	$(CC) $(CXXFLAG) $(INCL) -o $@ $^

terror: terror.cpp $(SOURCE)
	$(CC) $(CXXFLAG) $(INCL) -o $@ $^

tutils: tutils.cpp $(SOURCE)
	$(CC) $(CXXFLAG) $(INCL) -o $@ $^

tencode: tencode.cpp $(SOURCE)
	$(CC) $(CXXFLAG) $(INCL) -o $@ $^

tfile: tfile.cpp $(SOURCE)
	$(CC) $(CXXFLAG) $(INCL) -o $@ $^

ttoken: ttoken.cpp $(SOURCE)
	$(CC) $(CXXFLAG) $(INCL) -o $@ $^

ttype: ttype.cpp $(SOURCE)
	$(CC) $(CXXFLAG) $(INCL) -o $@ $^

tscope: tscope.cpp $(SOURCE)
	$(CC) $(CXXFLAG) $(INCL) -o $@ $^

tast: tast.cpp $(SOURCE)
	$(CC) $(CXXFLAG) $(INCL) -o $@ $^