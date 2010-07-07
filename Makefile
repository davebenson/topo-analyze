CFLAGS = `pkg-config --cflags glib-2.0`
LDFLAGS  = `pkg-config --libs glib-2.0` -lm

all: hough find-black
hough: hough.c
find-black: find-black.c
