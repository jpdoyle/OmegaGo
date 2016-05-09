#!/bin/bash

rebase=""
if [[ "$1" == "-r" ]]; then
    rebase="-r"
    shift
fi

./compress-archive.sh
git add -A
if [[ -z "$*" ]]; then
    git commit
else
    git commit -m "$*"
fi

git checkout master

while ! (git pull --no-edit $rebase &&
         git push); do
    sleep 1s
    [[ -n "$rebase" ]] && git rebase --abort
done

