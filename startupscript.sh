#!/bin/bash

# This would need to be changed to your lat and long.
wlsunset -t 3500 -T 4500 -l -41.2 -L 174.7
wal -R
awww-daemon
waybar
swaync
wl-paste --watch cliphist store