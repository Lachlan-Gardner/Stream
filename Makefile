PKG_CONFIG?=pkg-config
WAYLAND_PROTOCOLS=$(shell $(PKG_CONFIG) --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER=$(shell $(PKG_CONFIG) --variable=wayland_scanner wayland-scanner)

PKGS=wlroots-0.18 wayland-server wayland-protocols xkbcommon egl glesv2 gbm pixman-1
CFLAGS+= -g -Werror
LD_FLAGS= -Wl,--as-needed -Wl,--no-undefined

PKG_INCLUDES=$(shell $(PKG_CONFIG) --cflags $(PKGS)) -I/usr/local/include/scenefx
# I've changed the "-lscenefx-0.1" to "-lscenefx-0.2". Hopefully this doesn't break anything.
PKG_LIBS= -lscenefx-0.2 $(shell $(PKG_CONFIG) --libs $(PKGS) libinput libdrm) -lm -lrt

# LD_LIBS = -Wl,--start-group /usr/local/lib/libscenefx-0.1.so /usr/local/lib/libwlroots-0.18.so /usr/lib/libwayland-server.so /usr/lib/libdrm.so /usr/lib/libxkbcommon.so /usr/lib/libpixman-1.so -lm -lrt /usr/lib/libEGL.so /usr/lib/libgbm.so /usr/lib/libGLESv2.so /usr/lib/libinput.so -Wl,--end-group

# wayland-scanner is a tool which generates C headers and rigging for Wayland
# protocols, which are specified in XML. wlroots requires you to rig these up
# to your build system yourself and provide them in the include path.
xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

quackwm.o: quackwm.c xwayland.c types.h xdg-shell-protocol.h
	$(CC) $(CFLAGS) $(PKG_INCLUDES) -I. -DWLR_USE_UNSTABLE -o $@ -c $<
quackwm: quackwm.o
	$(CC) $< $(CFLAGS) $(PKG_INCLUDES) $(LD_FLAGS) $(PKG_LIBS) -o $@

clean:
	rm -f quackwm quackwm.o xdg-shell-protocol.h

.DEFAULT_GOAL=quackwm
.PHONY: clean
