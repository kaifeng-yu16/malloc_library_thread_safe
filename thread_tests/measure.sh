#!/bin/bash
make clean
make
loop=20
time_total=0
size_total=0

for((i=1;i<=$loop;i++));
do
text=`./thread_test_measurement`
echo $text
time=`echo $text | sed 's/^.*Time = //g' | sed 's/seconds.*$//g'`
size=`echo $text | sed 's/^.*Size = //g' | sed 's/ bytes//g'` 
echo $size
echo $time
time_total=`echo "$time_total + $time" | bc`
size_total=`echo "$size_total + $size" | bc`
echo $size_total
echo $time_total
done

size_ave=`echo "scale=1; $size_total / $loop" | bc`
time_ave=`echo "scale=6; $time_total / $loop" | bc`
echo ave size = $size_ave
echo ave time = $time_ave

