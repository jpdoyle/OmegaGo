#!/bin/bash
x=`pwd`;
(pushd build;
    make &&
    popd &&
    ./build/sgfparse --parsetest 2001-01-01-1.sgf) ||
        cd $x
