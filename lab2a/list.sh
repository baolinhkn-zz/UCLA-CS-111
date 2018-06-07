#!/bin/bash

> lab2_list.csv

ITER_1=(10 100 1000 10000 20000)

for j in "${ITER_1[@]}"
do
    ./lab2_list --iterations=$j --threads=1 >> lab2_list.csv
done

THREADS_2=(2 4 8 12)
ITER_2=(1 10 100 1000)
for i in "${THREADS_2[@]}"
do
    for j in "${ITER_2[@]}"
    do
	./lab2_list --threads=$i --iterations=$j >> lab2_list.csv
    done
done

ITER_2A=(1 2 4 8 16 32)
for i in "${THREADS_2[@]}"
do
    for j in "${ITER_2A[@]}"
    do
	./lab2_list --threads=$i --iterations=$j --yield=i >> lab2_list.csv
	./lab2_list --threads=$i --iterations=$j --yield=d >> lab2_list.csv
	./lab2_list --threads=$i --iterations=$j --yield=il >> lab2_list.csv
	./lab2_list --threads=$i --iterations=$j --yield=dl >> lab2_list.csv
    done
done

./lab2_list --threads=12 --iterations=32 --yield=i --sync=m >> lab2_list.csv
./lab2_list --threads=12 --iterations=32 --yield=d --sync=m >> lab2_list.csv
./lab2_list --threads=12 --iterations=32 --yield=il --sync=m >> lab2_list.csv
./lab2_list --threads=12 --iterations=32 --yield=dl --sync=m >> lab2_list.csv
./lab2_list --threads=12 --iterations=32 --yield=i --sync=s >> lab2_list.csv
./lab2_list --threads=12 --iterations=32 --yield=d --sync=s >> lab2_list.csv
./lab2_list --threads=12 --iterations=32 --yield=il --sync=s >> lab2_list.csv
./lab2_list --threads=12 --iterations=32 --yield=dl --sync=s >> lab2_list.csv

THREADS_4=(1 2 4 8 12 16 24)
for i in "${THREADS_4[@]}"
do
    ./lab2_list --threads=$i --iterations=1000 --sync=m >> lab2_list.csv
    ./lab2_list --threads=$i --iterations=1000 --sync=s >> lab2_list.csv
done
