# options and such
CC = g++
CFLAGS = -Wall -std=c++1z -w
DEPS = LibFS.h LibDisk.h
OBJ = LibDisk.o LibFS.o main_create.o
%.o: %.cpp $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<
main_create: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^
clean:
	-rm *.o $(OBJS) main_create
