#!/bin/bash

wal -R
awww-daemon
waybar
swaync
wl-paste --watch cliphist store
export SDL_VIDEODRIVER=wayland
export STEAM_FRAME_FORCE_WAYLAND=1