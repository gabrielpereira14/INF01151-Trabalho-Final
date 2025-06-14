CC = gcc
CFLAGS = -Wall -Wextra -g -lpthread

SRC_DIR = src
OBJ_DIR = obj

CLIENT_SRC = $(SRC_DIR)/client/client.c
CLIENT_OBJ = $(OBJ_DIR)/client/client.o
CLIENT_BIN = ./client

SERVER_SRC_DIR = $(SRC_DIR)/server
SERVER_SRC = $(wildcard $(SERVER_SRC_DIR)/*.c)
SERVER_OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SERVER_SRC))
SERVER_BIN = ./server

UTIL_SRC_DIR = $(SRC_DIR)/util
UTIL_SRC = $(wildcard $(UTIL_SRC_DIR)/*.c)
UTIL_OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(UTIL_SRC))

all: $(CLIENT_BIN) $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_OBJ) $(UTIL_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(SERVER_BIN): $(SERVER_OBJ) $(UTIL_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/client/%.o: $(SRC_DIR)/client/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/server/%.o: $(SRC_DIR)/server/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/util/%.o: $(SRC_DIR)/util/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(CLIENT_BIN) $(SERVER_BIN) out.* "user files"
