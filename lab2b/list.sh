#!/bin/bash

> lab2b_list.csv

THREADS_1=(1 2 4 8 12 16 24)

for i in "${THREADS_1[@]}"
do
    ./lab2_list --threads=$i --iterations=1000 --sync=m >>lab2b_list.csv
    ./lab2_list --threads=$i --iterations=1000 --sync=s >>lab2b_list.csv
done

THREADS_2=(1 2 4 8 16 24)

for i in "${THREADS_2[@]}"
do
    ./lab2_list --threads=$i --iterations=1000 --sync=m >> lab2b_list.csv
done

THREADS_3=(1 4 8 12 16)
ITER_3=(1 2 4 8 16)
ITER_3A=(10 20 40 80)

for i in "${THREADS_3[@]}"
do
    for j in "${ITER_3[@]}"
    do
	./lab2_list --lists=4 --threads=$i --iterations=$j --yield=id >>lab2b_list.csv
    done
    for j in "${ITER_3A[@]}"
    do
	./lab2_list --lists=4 --threads=$i --iterations=$j --yield=id --sync=m >>lab2b_list.csv
	./lab2_list --lists=4 --threads=$i --iterations=$j --yield=id --sync=s >> lab2b_list.csv
    done
done

THREADS_4=(1 2 4 8 12)
LISTS_4=(1 4 8 16)

for i in "${THREADS_4[@]}"
do
    for j in "${LISTS_4[@]}"
    do
	./lab2_list --iterations=1000 --threads=$i --lists=$j --sync=m >> lab2b_list.csv
	./lab2_list --iterations=1000 --threads=$i --lists=$j --sync=s >> lab2b_list.csv
    done
done
