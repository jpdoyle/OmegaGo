#!/bin/bash
for g in `cat ghc-hosts.txt`; do
    firefox --new-tab "http://$g:51337/games/" &
    sleep 2s
done
