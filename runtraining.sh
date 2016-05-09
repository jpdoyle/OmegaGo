cd /tmp
ls
mkdir feats
cd feats
ls
tar xzf ~/Dropbox/kgs-games/kgs-000.tgz
mkdir feats
cd feats
~/Dropbox/OmegaGo/OmegaGo/sgf-parse/build/sgfparse --features ../kgs-000/2001-01-26-2.sgf
ls
rm *
for f in ../kgs*/*; do while [[ `jobs | wc -l` -gt 5 ]]; do sleep 0.25s; done; (~/Dropbox/OmegaGo/OmegaGo/sgf-parse/build/sgfparse --features $f &); done; wait
ls
cd /tmp
ls
mkdir feats
cd feats
ls
tar xzf ~/Dropbox/kgs-games/kgs-000.tgz
mkdir feats
cd feats
~/Dropbox/OmegaGo/OmegaGo/sgf-parse/build/sgfparse --features ../kgs-000/2001-01-26-2.sgf
ls
rm *
for f in ../kgs*/*; do while [[ `jobs | wc -l` -gt 5 ]]; do sleep 0.25s; done; (~/Dropbox/OmegaGo/OmegaGo/sgf-parse/build/sgfparse --features $f &); done; wait
ls
ls *.txt | wc -l
pushd ~/private/omegago
git pull --no-edit
popd
tail -f /tmp/omegago-async-tourney.txt
ls | wc -l
ls
cd ..
ls
cd ..
ls
cd kgs-000
ls
cd ..
history | less
man history
history 1000
history 1000 | less
