OPTS=-fsanitize=address -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Werror -std=c17 -Wpedantic -O0 -g

all: http

http: http.o
	gcc $(OPTS) $^ -o $@

http.o: http.c
	gcc $(OPTS) -c $<

clean:
	rm -rf *.o http
