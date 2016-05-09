 cd /tmp/
 curl -O https\://www.python.org/ftp/python/2.7.11/Python-2.7.11.tgz
 tar xzf Python-2.7.11.tgz 
 cd Python-2.7.11
 ./configure && make -j12
 python.exe
 cd ../
 mkdir omegago
 cd omegago/
 git clone --recursive https://github.com/Ginto8/OmegaGo.git
 cd OmegaGo/
 cd ..
 # Select current version of virtualenv:
 VERSION=12.0.7
 # Name your first "bootstrap" environment:
 INITIAL_ENV=bootstrap
 # Set to whatever python interpreter you want for your first environment:
 PYTHON=$(which python)
 URL_BASE=https://pypi.python.org/packages/source/v/virtualenv
 # --- Real work starts here ---
 curl -O $URL_BASE/virtualenv-$VERSION.tar.gz
 tar xzf virtualenv-$VERSION.tar.gz
 # Create the first "bootstrap" environment.
 $PYTHON virtualenv-$VERSION/virtualenv.py $INITIAL_ENV
 # Don't need this anymore.
 rm -rf virtualenv-$VERSION
 # Install virtualenv into the environment.
 $INITIAL_ENV/bin/pip install virtualenv-$VERSION.tar.gz
 ls
 cd OmegaGo/
 VENV='../bootstrap/bin/virtualenv' ./run-worker.sh jpdoyle.net/omegago
 cd ..
