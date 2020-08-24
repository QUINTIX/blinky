#!/bin/sh

clang -std=c11 -c fish_dependentTests.c -o bin/fish_dependentTests.o \
-I../../include -I../fisheye

clang -fprofile-arcs -ftest-coverage -std=c11 -c ../fisheye/fishzoom.c -o bin/fishzoom.o \
-I../../include -I../fisheye

clang -fprofile-arcs -ftest-coverage -std=c11 -c ../fisheye/fishlens.c -o bin/fishlens.o \
-I../../include -I../fisheye

clang --coverage bin/fish_dependentTests.o bin/fishzoom.o bin/fishlens.o \
../../build/nqsw/mathlib.o -o a.out -L/usr/lib -L/usr/local/lib -lm -lcmocka

./a.out

lcov --directory . --base-directory . --capture -o cov.info
genhtml cov.info