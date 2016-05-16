#!/bin/bash

pushd $(dirname $0)

. ./build.sh || exit $?

cd server;
isDone=false
while ! $isDone; do
    python worker.py $*
    if [[ $? -eq 64 ]]; then
        pushd ..
        git stash
        git push
        git pull --no-edit
        git push
        git checkout master

        deactivate
        . ./build.sh || exit $?
        popd
    else
        isDone=true
    fi
done
cd -

deactivate

popd

