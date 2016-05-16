#!/bin/bash

pushd $(dirname $0)

. ./build.sh

cd server;
isDone=false
while ! $isDone; do
    python worker.py $*
    if [[ $? -eq 64 ]]; then
        cd -
        git stash
        git push
        git pull --no-edit
        git push
        git checkout master

        deactivate
        . ./build.sh
    else
        isDone=true
    fi
done
cd -

deactivate

popd

