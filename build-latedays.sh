#!/bin/bash
module load gcc-4.9.2

mkdir -p sgf-parse/build
cd sgf-parse/build
CC=gcc CXX=g++ cmake -DCMAKE_BUILD_TYPE=Release ..
make -j 8

