#!/bin/bash

pushd sgf-parse;
mkdir -p build
cd build/
if [[ "$1" != "debug" ]]; then
    cmake -DCMAKE_BUILD_TYPE=Release ..
else
    cmake -DCMAKE_BUILD_TYPE=Debug ..
fi
make -j $(nproc || echo 4);
popd;

pycmd='python2'
if ! which $pycmd; then
    pycmd='python'
fi

if ! ($pycmd --version | grep 'Python 2.7'); then
    echo "Need python 2.7"
    exit -1
fi

venvpath=$VENV
[[ -z "$venvpath" ]] && venvpath="virtualenv"
$venvpath --version || (echo "Need virtualenv"; exit -1)

if ! [[ -e venv ]]; then
    $venvpath -p $pycmd venv
fi

. venv/bin/activate

pip install -r requirements.txt


