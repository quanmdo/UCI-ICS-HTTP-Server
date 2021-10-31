##################################################
# Makefile template to build the define target
##################################################

# C compiler
CC = gcc
# C compiler flags
CFLAGS = -std=gnu11 -g -Wall -Wextra -Wpedantic -Wshadow -pthread
# Target executable name httpserver
TARGET = loadbalancer

all: $(TARGET)

# Used to build executable from .o file(s)
$(TARGET): $(TARGET).o List.o
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).o List.o

# Used to build .o file(s) from .c files(s)
$(TARGET).o: $(TARGET).c List.h
	$(CC) $(CFLAGS) -c $(TARGET).c
	
# using my own list ADT
List.o : List.c List.h
	$(CC) $(CFLAGS) -c List.c

# clean built artifacts except final executable
clean:
	rm -f $(TARGET).o List.o

spotless: clean
	-rm -f $(TARGET)

# Used to output a help message for the Makefile
help:
	@echo  "Usage: make [target] ...\n"
	@echo  "Miscellaneous:"
	@echo  "help\t\t\tShows this help\n"
	@echo  "Build:"
	@echo  "all\t\t\tBuild all the project\n"
	@echo  "Cleaning:"
	@echo  "clean\t\t\tRemove all intermediate objects"
