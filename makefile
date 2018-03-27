
.PHONY: clean all

clean:
	cd test/misc && $(MAKE) clean
	cd test/eval && $(MAKE) clean
	cd test/lexer && $(MAKE) clean
	cd test/parser && $(MAKE) clean
	cd tools && $(MAKE) clean

all:
	cd test/misc && $(MAKE) all
	cd test/eval && $(MAKE) all
	cd test/lexer && $(MAKE) all
	cd tools && $(MAKE) all