build:
	gcc -fPIC -Wextra -Wall -g -o so_scheduler.o -c scheduler.c
	gcc -fPIC -Wextra -Wall -g -o list.o -c list.c
	gcc -fPIC -Wextra -Wall -g -shared -o libscheduler.so so_scheduler.o list.o

clean:
	rm -f *.o
	rm -f *.so
