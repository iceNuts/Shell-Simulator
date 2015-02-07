CC = gcc 
CFLAGS = -Wall -g 

xssh: xssh.o

clean:
	rm -f xssh *.o
