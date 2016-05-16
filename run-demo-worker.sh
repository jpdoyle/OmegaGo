kill `pgrep -f worker.py`
kill `pgrep -f ' worker.sh'`
killall sgfparse
kill -9 `pgrep -f worker.py`
kill -9 `pgrep -f ' worker.sh'`
killall -9 sgfparse
rm -rf ~/.ccache/
rm -rf /run/user/2615888/*
cd /run/user/2615888
git clone --depth=1 --recursive git@github.com:Ginto8/OmegaGo.git
cd OmegaGo/
echo "$(nproc || echo 4) build threads"
./run-worker.sh jpdoyle.net/omegago ssl
