#!/bin/bash

mcc rbtree.c test.c -o test
./test
rm test
