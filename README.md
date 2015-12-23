# Torchvid

Utilities for loading videos into Torch using FFmpeg libraries.

    local torchvid = require('torchvid')

    local video =
      torchvid.Video.new('./centaur_1.mpg'):filter('yuv444p', 'scale=160x120,vflip')

    local tensor = video
      :next_video_frame()
      :to_byte_tensor()

    local yuv_tensor = tensor:float()
    yuv_tensor[{{2, 3}}]:csub(128)
    yuv_tensor:div(255)

## Installation

### Dependencies

* Torch
* pkg-config
* FFmpeg development libraries

In Ubuntu you can download the non-Torch dependencies with the following
command:

    apt-get install \
        pkg-config \
        libavformat-ffmpeg-dev \
        libavcodec-ffmpeg-dev \
        libavutil-ffmpeg-dev \
        libavfilter-ffmpeg-dev

### Building and installing Torchvid

    git clone https://github.com/anibali/torchvid.git /tmp/torchvid
    cd /tmp/torchvid
    luarocks make rockspecs/torchvid-scm-0.rockspec
    rm -rf /tmp/torchvid
