#!/bin/bash

# Update package list
sudo apt update

# Install packages that don't need version check
sudo apt install -y which sed binutils build-essential diffutils bash patch gzip bzip2 tar cpio unzip rsync bc findutils

# Check if 'file' exists in /usr/bin/file, if not install it
if [ ! -f /usr/bin/file ]; then
    sudo apt install -y file
fi

# Install 'make' (version 3.81 or later)
sudo apt install -y make

# Install 'gcc' and 'g++' (version 4.8 or later)
sudo apt install -y gcc g++

# Install 'perl' (version 5.8.7 or later)
sudo apt install -y perl

