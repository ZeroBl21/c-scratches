CC = clang
# C_LINKS = -I src
# C_FLAGS = -Wall -Wextra -pedantic -std=c99
# SRC = main.c src/*.c
# OUT = ./bin/kilo

# build: main.c
# 	@$(CC) $(SRC) -o $(OUT) $(C_FLAGS) $(C_FLAGS)

build: main.c
	@$(CC) main.c server.c -o ./bin/a $(C_FLAGS) $(C_FLAGS)

run: build
	./bin/a $(FILE)

build/server: server.c
	@$(CC) server.c -o ./bin/server $(C_FLAGS) $(C_FLAGS)

build/client: client.c
	@$(CC) client.c -o ./bin/client $(C_FLAGS) $(C_FLAGS)

build/client-udp: client-udp.c
	@$(CC) client-udp.c -o ./bin/client-udp $(C_FLAGS) $(C_FLAGS)

build/server-udp: server-udp.c
	@$(CC) server-udp.c -o ./bin/server-udp $(C_FLAGS) $(C_FLAGS)

build/server-chat: server-chat.c
	@$(CC) server-chat.c server.c -o ./bin/server-chat $(C_FLAGS) $(C_FLAGS)

run/server: build/server
	./bin/server 

run/client: build/client
	./bin/client $(HOST)

run/server-udp: build/server-udp
	./bin/server-udp 

run/client-udp: build/client-udp
	./bin/client-udp $(HOST) $(MSG)

run/server-chat: build/server-chat
	./bin/server-chat

clean:
	rm -f ./bin/
