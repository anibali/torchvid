TORCH_INCDIR ?= $(HOME)/torch/install/include
LUA_CFLAGS = -I$(TORCH_INCDIR)

FFMPEG_LIBS =   libavformat                        \
                libavfilter                        \
                libavcodec                         \
                libswresample                      \
                libswscale                         \
                libavutil                          \

CFLAGS ?= -O3 -Wall
CFLAGS := $(shell pkg-config --cflags $(FFMPEG_LIBS)) $(CFLAGS)
LDLIBS := $(shell pkg-config --libs $(FFMPEG_LIBS)) $(LDLIBS)

all: csrc/torchvidc.so

%.o: %.c
	$(CC) -c $(CFLAGS) -fPIC $(LUA_CFLAGS) -o $@ $<

csrc/torchvidc.so: csrc/torchvidc.o
	$(CC) -shared csrc/torchvidc.o $(LDLIBS) -o $@

clean:
	rm -f csrc/torchvidc.so csrc/torchvidc.o *.rock
