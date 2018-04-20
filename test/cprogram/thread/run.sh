#!/bin/bash

../../../mcc test.c -o test -lpthread
./test
rm test
