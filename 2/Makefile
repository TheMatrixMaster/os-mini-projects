all: test1 test2 test3 test4 test5

test1: sut.o test1.o
	gcc -o test1 sut.o test1.o 

test2: sut.o test2.o
	gcc -o test2 sut.o test2.o

test3: sut.o test3.o
	gcc -o test3 sut.o test3.o

test4: sut.o test4.o
	gcc -o test4 sut.o test4.o

test5: sut.o test5.o
	gcc -o test5 sut.o test5.o

sut.o: sut.c
	gcc -c sut.c -lpthread

test1.o: test1.c
	gcc -c test1.c

test2.o: test2.c
	gcc -c test2.c
	
test3.o: test3.c
	gcc -c test3.c

test4.o: test4.c
	gcc -c test4.c

test5.o: test5.c
	gcc -c test5.c

