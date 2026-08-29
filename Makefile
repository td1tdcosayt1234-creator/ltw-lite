# ltw-lite — a GL→GLES "thin wrapper" that keeps mods like Create from crashing
#
# Default target: build & run the headless translator tests (no GPU needed).
# `gles` target is for an Android/embedded build with GLES/EGL dev libs.

CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra

TESTS = test_glsl

all: test

test: $(TESTS)
	./test_glsl

test_glsl: glsl_translate.c test_glsl.c glsl_translate.h
	$(CC) $(CFLAGS) -o $@ glsl_translate.c test_glsl.c

# Requires GLES/EGL headers + libs (e.g. on an Android NDK / Mesa host).
gles: gl_wrapper.c glsl_translate.c
	$(CC) -DLTW_HAVE_GLES -lGLESv2 -lEGL -o ltw-gles gl_wrapper.c glsl_translate.c

clean:
	rm -f test_glsl ltw-gles

.PHONY: all test gles clean
