#!/bin/bash

gcc -o raytetris.exe main.c  -I include -L lib -lraylib -lgdi32 -lwinmm -ggdb

./raytetris.exe
