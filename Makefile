all: dirs bin/pong bin/cube bin/cobra

dirs:
	mkdir -p bin/

bin/pong: src/pong.c
	gcc -o $@ $^ -O2 -g -lao -ldl -lm


bin/cube: src/cube.c
	gcc -o $@ -g $^ -O2 -lao -ldl -lm

bin/cobra: src/cobra.c
	gcc -o $@ -g $^ -O2 -lao -ldl -lm

