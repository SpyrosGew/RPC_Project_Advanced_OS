CC = gcc
CFLAGS = -Wall -g
OBJ = main.o functions.o
TARGET = program

all: $(TARGET)
	@rm -f $(OBJ)  

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
