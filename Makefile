# complier
CC = gcc

# compiler flags
CFLAGS = -g -Wall

# building target
TARGET = server

# source file
SOURCE = main

# clean
RM = rm

all: $(TARGET)
	chmod 777 $(TARGET)

$(TARGET): $(SOURCE).c
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE).c

clean:
	$(RM) $(TARGET)
