#!/bin/bash
gcc `pkg-config --cflags gtk4` -o app main.c `pkg-config --libs gtk4`