#!/bin/bash

if [ ${EUID:-0} -ne 0 ] || [ "$(id -u)" -ne 0 ]; then
    echo You need root
    exit 1 # Exits using generic error
fi

if [ -f /etc/os-release ]; then
  . /etc/os-release
  #echo "$ID and $VERSION_ID"
  if [[ ! "$ID" = "ubuntu" || ! "$VERSION_ID" = "24.04" ]]; then
    printf "This script was designed for Ubuntu 24.04 LTS. \nAre you sure you want to continue? (y/n) "
    read -r CONT
    if [[ ! $CONT =~ ^[Yy]$ ]]; then
      exit 1
    fi
  fi
fi

gst_pot_paths=(
  "/usr/lib/x86_64-linux-gnu/gstreamer-1.0/"
  "/usr/lib/aarch64-linux-gnu/gstreamer-1.0"
)

for i in "${gst_pot_paths[@]}"; do
  if [ -f "$i/libgstrswebrtc.so" ]; then
    printf "You seem to already have the library installed at: \n   $i/libgstrswebrtc.so. \n\nAre you sure you want to continue? (y/n) "
    read -r CONT
    if [[ ! $CONT =~ ^[Yy]$ ]]; then
      exit 1
    fi
  fi
done



# Get plugin repo
git clone https://github.com/GStreamer/gst-plugins-rs
cd gst-plugins-rs

# Setup rust building 
sudo apt install rustup -y
sudo rustup default stable
sudo cargo install cargo-c

# Install and build the plugin
sudo cargo cinstall -p gst-plugin-rswebrtc --prefix=/usr
# sudo cargo cinstall -p gst-plugin-webrtchttp --prefix=/usr # Depricated