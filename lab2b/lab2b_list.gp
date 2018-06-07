#! /usr/bin/gnuplot
#
# purpose:
#	 generate data reduction graphs for the multi-threaded list project
#
# input: lab2b_list.csv
#	1. test name
#	2. # threads
#	3. # iterations per thread
#	4. # lists
#	5. # operations performed (threads x iterations x (ins + lookup + delete))
#	6. run time (ns)
#	7. run time per operation (ns)
#	8. wait-for-lock time (ns)
#
# output:
#	lab2b_1.png ... throughput and number of threads
#	lab2b_2.png ... wait-for-lock time and average time per operation and number of competing threads
#	lab2_list-3.png ... threads and iterations that run (protected) w/o failure
#	lab2_list-4.png ... cost per operation vs number of threads
#
# Note:
#	Managing data is simplified by keeping all of the results in a single
#	file.  But this means that the individual graphing commands have to
#	grep to select only the data they want.
#
#	Early in your implementation, you will not have data for all of the
#	tests, and the later sections may generate errors for missing data.
#

# general plot parameters
set terminal png
set datafile separator ","

#throughput and number of threads
set title "Listb-1: Throughput vs. Number of Threads"
set xlabel "Threads"
set logscale x 10
set ylabel "Throughput (op/s)"
set logscale y 10
set output 'lab2b_1.png'

# grep out only single threaded, un-protected, non-yield results
plot \
     "< grep -e 'list-none-m,[0-9]*,1000,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'mutex' with linespoints lc rgb 'red', \
     "< grep -e 'list-none-s,[0-9]*,1000,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'sink' with linespoints lc rgb 'green'


set title "Listb-2: Wait-For-Lock Time and Average Time Per Operation vs. Number of Threads"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Time (ns)"
set logscale y 10
set output 'lab2b_2.png'

plot \
     "< tail -n +15 lab2b_list.csv | grep list-none-m,[0-9]*,1000" using ($2):($8) \
	title 'wait-for-lock time' with linespoints lc rgb 'violet', \
     "< tail -n +15 lab2b_list.csv | grep list-none-m,[0-9]*,1000" using ($2):($7) \
	title 'average time per operation' with linespoints lc rgb 'orange', \
     
set title "Listb-3: Protected Iterations that run without failure"
unset logscale x
set xrange [0:20]
set xlabel "Threads"
set xtics("" 0, "1" 1, "4" 4, "8" 8, "12" 12, "16" 16)
set ylabel "successful iterations"
set logscale y 10
set output 'lab2b_3.png'
plot \
    "< grep 'list-id-none,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
	with points lc rgb "red" title "unprotected", \
    "< grep 'list-id-m,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
	with points lc rgb "green" title "Mutex", \
    "< grep 'list-id-s,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
	with points lc rgb "blue" title "Spin-Lock", \
#
# "no valid points" is possible if even a single iteration can't run
#

# unset the kinky x axis
unset xtics
set xtics

set title "Listb-4: Throughput vs. Number of Threads (mutex)"
set xlabel "Threads"
set xrange [0:20]
set xtics("" 0, "1" 1, "4" 4, "8" 8, "12" 12, "16" 16)
set ylabel "Throughput (op/s)"
set logscale y 10
set output 'lab2b_4.png'
plot \
     "< tail -n +70 lab2b_list.csv | grep -e 'list-none-m,[0-9]*,1000,1,'" using ($2):(1000000000/($7)) \
	title 'lists=1' with linespoints lc rgb 'blue', \
     "< grep -e 'list-none-m,[0-9]*,1000,4' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=4' with linespoints lc rgb 'green', \
     "< grep -e 'list-none-m,[0-9]*,1000,8' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=8' with linespoints lc rgb 'yellow', \
     "< grep -e 'list-none-m,[0-9]*,1000,16' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=16' with linespoints lc rgb 'orange', \

set title "Listb-5: Throughput vs. Number of Threads (spin)"
set xlabel "Threads"
set xrange [0:20]
set xtics("" 0, "1" 1, "4" 4, "8" 8, "12" 12, "16" 16)
set ylabel "Throughput (op/s)"
set logscale y 10
set output 'lab2b_5.png'
plot \
     "< tail -n +70 lab2b_list.csv | grep -e 'list-none-s,[0-9]*,1000,1,'" using ($2):(1000000000/($7)) \
	title 'lists=1' with linespoints lc rgb 'blue', \
     "< grep -e 'list-none-s,[0-9]*,1000,4' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=4' with linespoints lc rgb 'green', \
     "< grep -e 'list-none-s,[0-9]*,1000,8' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=8' with linespoints lc rgb 'yellow', \
     "< grep -e 'list-none-s,[0-9]*,1000,16' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=16' with linespoints lc rgb 'orange', \
