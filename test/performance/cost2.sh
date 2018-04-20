#!/bin/bash

begin_time=$(date +%s%N)
../../mcc -S performance.c
compiled_time=$(date +%s%N)

compile_cost=$((($compiled_time - $begin_time)/1000000))
echo "mcc compile cost: $compile_cost ms"

begin_time=$(date +%s%N)
gcc -S performance.c
compiled_time=$(date +%s%N)

compile_cost=$((($compiled_time - $begin_time)/1000000))
echo "gcc compile cost: $compile_cost ms"

begin_time=$(date +%s%N)
clang -S performance.c
compiled_time=$(date +%s%N)

compile_cost=$((($compiled_time - $begin_time)/1000000))
echo "clang compile cost: $compile_cost ms"

rm performance.s