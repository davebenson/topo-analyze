CFLAGS = `pkg-config --cflags glib-2.0` -g
LDFLAGS  = `pkg-config --libs glib-2.0` -lm -g

all: hough find-black
hough: hough.c
find-black: find-black.c
