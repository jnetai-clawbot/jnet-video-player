CFLAGS = -O3 -march=armv8-a+crc+simd+crypto -mtune=cortex-a76 -flto -ffast-math -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
LDFLAGS = -lwayland-client -lwayland-egl -lwayland-cursor -lEGL -lGLESv2 -lxkbcommon -lavcodec -lavformat -lavutil -lswscale -lswresample -lm -lpthread -lrt

PROTOCOL_DIR = /usr/share/wayland-protocols

XDG_SHELL_XML = $(PROTOCOL_DIR)/stable/xdg-shell/xdg-shell.xml
XDG_ACTIVATION_XML = $(PROTOCOL_DIR)/staging/xdg-activation/xdg-activation-v1.xml

OBJS = main.o player.o renderer.o ui.o playlist.o xdg-shell.o xdg-activation-v1.o

all: xdg-shell.h xdg-activation-v1.h jnet-video

jnet-video: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

xdg-shell.h: $(XDG_SHELL_XML)
	wayland-scanner client-header $< $@

xdg-shell.c: $(XDG_SHELL_XML)
	wayland-scanner private-code $< $@

xdg-activation-v1.h: $(XDG_ACTIVATION_XML)
	wayland-scanner client-header $< $@

xdg-activation-v1.c: $(XDG_ACTIVATION_XML)
	wayland-scanner private-code $< $@

clean:
	rm -f jnet-video *.o xdg-shell.c xdg-shell.h xdg-activation-v1.c xdg-activation-v1.h

.PHONY: all clean
