CC = clang
# C_LINKS = -I src
# C_FLAGS = -Wall -Wextra -pedantic -std=c99
# SRC = main.c src/*.c
# OUT = ./bin/kilo

# build: main.c
# 	@$(CC) $(SRC) -o $(OUT) $(C_FLAGS) $(C_FLAGS)

build: main.c
	@$(CC) main.c -o ./bin/a $(C_FLAGS) $(C_FLAGS)

run: build
	./bin/a $(FILE)

build/server: server.c
	@$(CC) server.c -o ./bin/server $(C_FLAGS) $(C_FLAGS)

run/server: build/server
	./bin/server 

clean:
	rm -f ./bin/
