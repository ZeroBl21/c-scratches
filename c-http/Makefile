CC = clang
# C_LINKS = -I src
C_FLAGS = -Wall -Wextra -pedantic -std=c11
SRC = ./src/*.c
OUT = ./bin/a

build: ./src/main.c
	@$(CC) $(SRC) -o $(OUT) $(C_FLAGS) 

run: build
	./bin/a

http/get: 
	curl -v http://localhost:3490/

http/post:
	curl -X POST -v http://localhost:3490/create \
		-H "Content-Type: application/json" \
		-d '{"input": "let x = 10;"}'

clean:
	rm -rf ./bin/*
