CC=gcc
CFLAGS=-g -Wall -Wextra -lpthread
ADD=lab2_add
LIST=lab2_list
LIST_SOURCE=SortedList
LAB2_ADD_GRAPHS= $(ADD)-1.png $(ADD)-2.png $(ADD)-3.png $(ADD)-4.png $(ADD)-5.png
LAB2_LIST_GRAPHS= $(LIST)-1.png $(LIST)-2.png $(LIST)-3.png $(LIST)-4.png

build: $(ADD) $(LIST)

$(ADD): $(ADD).c
	@$(CC) $(CFLAGS) -o $(ADD) $(ADD).c

$(LIST): $(LIST).c $(LIST_SOURCE).c
	@$(CC) -o $(LIST) $(CFLAGS) $(LIST).c $(LIST_SOURCE).c

clean:
	@rm -f $(ADD) $(LIST) lab2a-104732121.tar.gz

tests:
	@./add.sh
	@./list.sh
graphs:
	@./$(ADD).gp
	@./$(LIST).gp

make dist: clean build tests graphs
	@tar -czf lab2a-104732121.tar.gz README Makefile \
	$(LAB2_ADD_GRAPHS) $(ADD).gp $(ADD).c $(ADD).csv add.sh \
	$(LIST).c $(LIST_SOURCE).h $(LIST_SOURCE).c list.sh $(LIST).gp $(LIST).csv $(LAB2_LIST_GRAPHS)
