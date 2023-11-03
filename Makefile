OBJS := $(patsubst src/%.c, bin/%.o, $(wildcard src/*.c))

bin/%.o: src/%.c
	gcc -Wall -Wextra -Wpedantic -std=c17 -fsanitize=address -fsanitize=undefined -c $< -o $@

bin/r5: $(OBJS)
	gcc $? -o $@ -fsanitize=address -fsanitize=undefined

all: bin/r5