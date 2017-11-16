CFLAGS = -gz=none -Wall -g
LDFLAGS =  -lao -ldl -lm

all: dirs bin/pong bin/cube bin/cobra bin/invader bin/invader2

dirs:
	mkdir -p bin/
	mkdir -p obj/

clean:
	rm -rf bin/
	rm -rf obj/

bin/pong: src/pong.c
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)


bin/cube: src/cube.c
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

bin/cobra: src/cobra.c
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

bin/invader: obj/invader.o
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

bin/invader2: obj/invader2.o obj/invader_model.o obj/matrix.o obj/ship_model.o obj/ball_model.o
	gcc $(CFLAGS) -o $@  $^ $(LDFLAGS)



obj/%.o: src/%.c
	gcc $(CFLAGS) -c -o $@ $^

