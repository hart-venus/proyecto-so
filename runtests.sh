#!/usr/bin/env bash

gcc copy.c -o copy

if [ $? -eq 0 ]; then
    ./copy arg2 arg3 # TODO: argumentos relevantes a directorios de prueba

    rm copy
fi