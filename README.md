# Stream
Strea(w)m

This project requires:   
    - wayland   
    - river   
    - xkbcommon   
    - meson  
    - pkg-config  
    - ninja  

Build using:
    
    meson setup build
    ninja -C build

Run using:  

    river -c ./build/stream

Or just run flow.sh, though you might need to run:
    
    chmod +x ./flow.sh


Goals:
 - [ ] Bring over keybinds.
 - [ ] Brightness and volume controls.
 - [ ] Wallpaper.
 - [ ] Change cursor on resize and move.
 - [ ] Proper alt tab.
 - [ ] Screen on and off toggle.
 - [ ] Better resize.
 - [ ] Minimise.
 - [ ] Maximise.
 - [ ] Fullscreen.
 - [ ] Taskbar.
 - [ ] Exclusion zone.
 - [ ] Psuedotiling. (Possibly a pipe dream)
