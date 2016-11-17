all: dirs bin/pong bin/cube

dirs:
	mkdir -p bin/

bin/pong: src/pong.c
	gcc -o $@ $^ -lao -ldl -lm


bin/cube: src/cube.c
	gcc -o $@ -g $^ -lao -ldl -lm