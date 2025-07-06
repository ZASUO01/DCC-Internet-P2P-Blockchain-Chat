ADDR4 = 150.164.213.243
ADDR6 = 2804:1f4a:dcc:ff03::1

CC = gcc
CFLAGS = -Wall -Wextra -Iinclude 
LDFLAGS = -lssl -lcrypto -lm -g

BIN = bin
OBJ = obj
SRC = src
INCLUDE = include

SRCS = $(wildcard $(SRC)/*.c)
OBJS = $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SRCS))

TARGET = $(BIN)/main

all: $(TARGET) 

$(TARGET): $(OBJS) | $(BIN)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
		
$(OBJ)/%.o: $(SRC)/%.c | $(OBJ)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN) $(OBJ): 
	mkdir -p $@

run:
	$(TARGET) $(ADDR4)

clean:
	rm -rf $(BIN) $(OBJ) $(LOG)