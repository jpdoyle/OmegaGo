#!/bin/bash
cd server/gamerecords
time=$(date +'%Y-%m-%d-%H-%M-%S')
file="${time}-`hostname`.tgz"
if tar czf "$file" *.sgf; then
    rm *.sgf
else
    rm $file
fi
cd -
