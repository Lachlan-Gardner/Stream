build: stream.c
	meson setup build
	ninja -C build

run:
	river -c "~/stream/build/stream"