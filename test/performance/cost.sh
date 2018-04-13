#!/bin/bash

begin_time=$(date +%s%N)
../../mcc performance.c -o performance
compiled_time=$(date +%s%N)
./performance
run_time=$(date +%s%N)

compile_cost=$(($compiled_time - $begin_time))
run_cost=$(($run_time - $compiled_time))
echo "mcc compile cost: $compile_cost ns"
echo "mcc run cost: $run_cost ns"

begin_time=$(date +%s%N)
gcc performance.c -o performance
compiled_time=$(date +%s%N)
./performance
run_time=$(date +%s%N)

compile_cost=$(($compiled_time - $begin_time))
run_cost=$(($run_time - $compiled_time))
echo "gcc compile cost: $compile_cost ns"
echo "gcc run cost: $run_cost ns"

rm performance