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

if ! ($pycmd --version 2>&1 | grep '^Python 2\.7'); then
    echo "Need python 2.7"
    exit -1
fi

venvpath=$VENV
[[ -z "$venvpath" ]] && venvpath="virtualenv"
if ! ($venvpath --version); then
    echo
    echo "Grabbing virtualenv"
    echo
    wget https://raw.github.com/pypa/virtualenv/master/virtualenv.py
    venvpath="$pycmd virtualenv.py"
    $venvpath --version || exit -1
fi

if ! [[ -e venv ]]; then
    $venvpath -p $pycmd venv
fi

. venv/bin/activate

pip install -r requirements.txt


