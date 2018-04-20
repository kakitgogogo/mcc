#!/bin/bash

../../../mcc test.c -o test
./test
rm test
