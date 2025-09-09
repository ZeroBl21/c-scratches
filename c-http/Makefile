CC = clang
# C_LINKS = -I src
C_FLAGS = -Wall -Wextra -pedantic -std=c11
SRC = ./src/*.c
OUT = ./bin/a

build: ./src/main.c
	@$(CC) $(SRC) -o $(OUT) $(C_FLAGS) 

run: build
	./bin/a

test: 
	curl -v http://localhost:3490/

clean:
	rm -f ./bin/
