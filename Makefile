#
# Marryampic II - Makefile
#
# By Marc Le Douarain - 29 December 2002
#
# Last update 28 January 2003
#

CFLAGS := $(shell sdl-config --cflags)

# uncomment if your SDL_image can display IFF ILBM 24bits pictures
CFLAGS += -DILBM_24BITS_SUPPORT

LDFLAGS := $(shell sdl-config --libs)
LDFLAGS += -lSDL_image -lSDL_mixer

all: marryampic2


marryampic2: marryampic2.o load_aiff.o my_load_sound.o SFont.o
	g++ -o $@ marryampic2.o $(LDFLAGS) load_aiff.o my_load_sound.o SFont.o

marryampic2.o: marryampic2.cpp
	g++ -c $< -o $@ $(CFLAGS)
load_aiff.o: load_aiff.c
	gcc -c $< -o $@ $(CFLAGS)
my_load_sound.o: my_load_sound.c
	gcc -c $< -o $@ $(CFLAGS)
SFont.o: SFont.c
	gcc -c $< -o $@ $(CFLAGS)


clean:
	rm -f *.o

dist: clean
	(cd ..;rm -f marryampic2.zip;zip -r marryampic2.zip marryampic2/*)
