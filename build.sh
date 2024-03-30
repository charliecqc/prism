#!/bin/bash -e

#sudo make clean; ./PRISM/build.sh; make -j $(nproc);
sudo make clean; ./PRISM/build.sh; make -j 1;
