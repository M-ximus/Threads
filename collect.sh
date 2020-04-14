#!/bin/bash
exec 1> out.txt
exec 2> err.txt
for num_thr in 1 2 3 4 5 6 7 8
do
time ./integr.out $num_thr
done

for num_thr in 1 2 3 4 5 6 7 8
do
time ./integr.out $num_thr
done

