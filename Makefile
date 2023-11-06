SRCS := core.c
OBJS := $(addprefix bin/, $(patsubst %.c, %.o, $(notdir $(SRCS))))

.PHONY: all clean

all: bin/r5

clean:
	rm bin/*

bin/r5: $(OBJS)
	gcc $? -o $@ -fsanitize=address -fsanitize=undefined

bin/%.o: src/%.c | bin
	gcc -Wall -Wextra -Wpedantic -std=c17 -fsanitize=address -fsanitize=undefined -c $< -o $@

bin:
	mkdir -p bin