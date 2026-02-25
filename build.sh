#!/bin/bash

gcc -o rayblocks.exe main.c  -I include -L lib -lraylib -lgdi32 -lwinmm -mwindows -ggdb

./rayblocks.exe
