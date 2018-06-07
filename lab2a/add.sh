#!/bin/bash

> lab2_add.csv

THREADS_1=(2 4 8 12)
ITER_1=(10 20 40 80 1000 10000 100000)

for i in "${THREADS_1[@]}"
do
    for j in "${ITER_1[@]}"
    do
	./lab2_add --yield --threads=$i --iterations=$j >> lab2_add.csv
    done
done

THREADS_2=(2 8)
ITER_2=(100 1000 10000 100000)
for i in "${THREADS_2[@]}"
do
    for j in "${ITER_2[@]}"
    do
	./lab2_add --yield --threads=$i --iterations=$j >> lab2_add.csv
	./lab2_add --threads=$i --iterations=$j >> lab2_add.csv
    done
done

ITER_3=(10 100 10000 100000)
for j in "${ITER_3[@]}"
do
    ./lab2_add --iterations=$j --threads=1>> lab2_add.csv
done

THREADS_4=(2 4 8 12)
for i in "${THREADS_4[@]}"
do
    ./lab2_add --yield --threads=$i --iterations=10000 --sync=m >> lab2_add.csv
done

for i in "${THREADS_4[@]}"
do
    ./lab2_add --yield --threads=$i --iterations=1000 --sync=s >> lab2_add.csv
done

for i in "${THREADS_4[@]}"
do
    ./lab2_add --yield --threads=$i --iterations=10000 --sync=c >> lab2_add.csv
done


THREADS_5=(1 2 4 8 12)
for i in "${THREADS_5[@]}"
do
    ./lab2_add --threads=$i --iterations=10000 >> lab2_add.csv
done

for i in "${THREADS_5[@]}"
do
    ./lab2_add --threads=$i --iterations=10000 --sync=m >> lab2_add.csv
done

for i in "${THREADS_5[@]}"
do
    ./lab2_add --threads=$i --iterations=10000 --sync=s >> lab2_add.csv
done

for i in "${THREADS_5[@]}"
do
    ./lab2_add --threads=$i --iterations=10000 --sync=c >> lab2_add.csv
done
