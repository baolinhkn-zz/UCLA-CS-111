CC=gcc
CFLAGS=-g -Wall -Wextra -lpthread -lprofiler
LIST=lab2_list
LIST2B=lab2b_list
LIST_SOURCE=SortedList
LAB2_LIST_GRAPHS= lab2b_1.png lab2b_2.png lab2b_3.png lab2b_4.png lab2b_5.png

build:  $(LIST)

$(LIST): $(LIST).c $(LIST_SOURCE).c
	@$(CC) -o $(LIST) $(CFLAGS) $(LIST).c $(LIST_SOURCE).c

clean:
	@rm -f RAWprofile.out $(LIST) lab2b-104732121.tar.gz

tests:
	@./list.sh

graphs:
	@./$(LIST2B).gp

profile:
	@touch RAWprofile.out
	@touch profile.out
	@>RAWprofile.out
	@>profile.out
	@export CPUPROFILE=RAWprofile.out lab2_list; \
	./lab2_list --threads=12 --iterations=1000 --sync=s; \
	pprof --text lab2_list RAWprofile.out > profile.out; \
	pprof --list=threadHandler lab2_list RAWprofile.out >> profile.out

make dist: clean build tests graphs profile
	@tar -czf lab2b-104732121.tar.gz README Makefile \
	$(LIST).c $(LIST_SOURCE).h $(LIST_SOURCE).c list.sh $(LIST2B).gp $(LIST2B).csv $(LAB2_LIST_GRAPHS) profile.out
