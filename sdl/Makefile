# make -C sdl: the SDL2 frontend (sdlpop2) over the core and the renderer
CFLAGS ?= -O2 -Wall
SRC = main.c $(wildcard ../src/*.c)
sdlpop2: $(SRC) $(wildcard ../src/*.h)
	$(CC) $(CFLAGS) $(shell sdl2-config --cflags) -o $@ $(SRC) $(shell sdl2-config --libs) -lm
clean:
	rm -f sdlpop2
