#!/bin/bash

mcc test.c leptjson.c -o test
./test
rm test
