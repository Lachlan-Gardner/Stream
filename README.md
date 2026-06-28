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

# Waybar
   
This repo comes with a basic waybar config, simply install waybar, run:  
        
     
    ln -s ~/Stream/Waybar/* ~/.config/waybar
         
This will symlink the config files for waybar to the correct location. The folder must exist and there musn't be any files or directories by the same names though.  
If you get an error about cava maybe being endless, just make cava.sh in the Scripts directory executable using chmod (`chmod +x /path/to/cava.sh`).

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
 - [x] Rounded corners via scenefx.
 - [x] Window size logic on window creation. (Sort of).
 - [x] Gamma control.
 - [ ] Reposition.
 - [ ] Pywal integration (probably just border focus colours). 
 - [x] Screenshotting.
 - [ ] Idle and locking managers.
 - [x] Taskbar.
 - [x] Autostart.
 - [ ] Workspaces.
 - [ ] Psuedotiling. (Possibly a pipe dream)
