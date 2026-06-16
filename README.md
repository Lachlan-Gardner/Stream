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
    ninja -c build

Run using:   
    `river -c ./build/stream`


Goals:
 - [] Bring over keybinds.
 - [] Warp cursor on resize, dwl better resize style.
 - [] Minimise.
 - [] Maximise.
 - [] Fullscreen.
 - [] Taskbar.
 - [] Exclusion zone.
 - [] Psuedotiling. (Possibly a pipe dream)
