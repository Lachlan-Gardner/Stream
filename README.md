# Stream
Strea(w)m

Now using dwl (though the name doesn't work now, might need to change that).
This project requires:
    - libinput
    - wayland
    - wlroots (compiled with the libinput backend)
    - xkbcommon
    - wayland-protocols (compile-time only)
    - pkg-config (compile-time only)

dwl has the following additional dependencies if XWayland support is enabled:
    - libxcb
    - libxcb-wm
    - wlroots (compiled with X11 support)
    - Xwayland (runtime only)


Build for testing using:   
     
    make clean all
         


This will copy config.def.h to config.h (which is what dwl.c actually includes) and then compile dwl.c to dwl. You then run dwl

Goals:
 - [x] Bring over keybinds.
 - [x] Brightness and volume controls.
 - [x] Wallpaper.
 - [x] Change cursor on resize and move.
 - [x] Proper alt tab.
 - [x] Screen on and off toggle.
 - [x] Better resize.
 - [x] Minimise.
 - [x] Maximise.
 - [x] Fullscreen.
 - [ ] Pywal integration (probably just border focus colours). 
 - [ ] Screenshotting.
 - [ ] Idle and locking managers.
 - [ ] Taskbar.
 - [ ] Autostart.
 - [ ] Workspaces.
 - [ ] Psuedotiling. (Possibly a pipe dream)
