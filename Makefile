# options and such
CC = g++
CFLAGS = -Wall -std=c++1z -w
DEPS = LibFS.h LibDisk.h
OBJ = LibDisk.o LibFS.o main.o
%.o: %.cpp $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<
main: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^
clean:
	-rm *.o $(OBJS) main
