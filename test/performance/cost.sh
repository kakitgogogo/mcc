#!/bin/bash

begin_time=$(date +%s%N)
../../mcc performance.c -o performance
compiled_time=$(date +%s%N)
./performance
run_time=$(date +%s%N)

compile_cost=$((($compiled_time - $begin_time)/1000000))
run_cost=$((($run_time - $compiled_time)/1000000))
echo "mcc compile cost: $compile_cost ms"
echo "mcc run cost: $run_cost ms"

begin_time=$(date +%s%N)
gcc performance.c -o performance
compiled_time=$(date +%s%N)
./performance
run_time=$(date +%s%N)

compile_cost=$((($compiled_time - $begin_time)/1000000))
run_cost=$((($run_time - $compiled_time)/1000000))
echo "gcc compile cost: $compile_cost ms"
echo "gcc run cost: $run_cost ms"

begin_time=$(date +%s%N)
clang performance.c -o performance
compiled_time=$(date +%s%N)
./performance
run_time=$(date +%s%N)

compile_cost=$((($compiled_time - $begin_time)/1000000))
run_cost=$((($run_time - $compiled_time)/1000000))
echo "clang compile cost: $compile_cost ms"
echo "clang run cost: $run_cost ms"

rm performance