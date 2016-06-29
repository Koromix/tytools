#!/bin/sh

sudo add-apt-repository -y ppa:george-edison55/cmake-3.x
sudo apt-get update -qq
sudo apt-get install -qqy cmake libudev-dev qtbase5-dev
