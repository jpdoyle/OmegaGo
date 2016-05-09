#!/bin/bash

pushd sgf-parse;
make;
cd build/
if [[ "$1" != "debug" ]]; then
    cmake -DCMAKE_BUILD_TYPE=Release ..
else
    cmake -DCMAKE_BUILD_TYPE=Debug ..
fi
make -j`nproc`;
popd;

pycmd='python2'
if ! which $pycmd; then
    pycmd='python'
fi

if ! [[ -e venv ]]; then
    virtualenv -p $pycmd venv
fi

. venv/bin/activate

pip install -r requirements.txt

cd server;
python server.py;
cd -

deactivate

