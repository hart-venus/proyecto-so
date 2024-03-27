#!/usr/bin/env bash

gcc copy.c -o copy

if [ $? -eq 0 ]; then
    ./copy test_input test_output

    rm copy
fi