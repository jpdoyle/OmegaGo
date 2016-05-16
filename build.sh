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

if ! [[ -e venv ]]; then
    echo
    echo "Grabbing virtualenv"
    echo
    wget https://raw.github.com/pypa/virtualenv/master/virtualenv.py

    $pycmd virtualenv.py --no-setuptools --no-pip --no-wheel venv
    . venv/bin/activate
    wget https://bootstrap.pypa.io/get-pip.py
    python get-pip.py
    pip --version || exit -1
else
    . venv/bin/activate
fi

pip install -r requirements.txt


