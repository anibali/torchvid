# torchvid

Utilities for loading videos into Torch using FFmpeg libraries.

```lua
local torchvid = require('torchvid')

local video =
  torchvid.Video.new('./centaur_1.mpg'):filter('yuv444p', 'scale=160x120,vflip')

local tensor = video
  :next_video_frame()
  :to_byte_tensor()

local yuv_tensor = tensor:float()
yuv_tensor[2]:csub(128)
yuv_tensor[3]:csub(128)
yuv_tensor:div(255)
```

## Installing

```sh
git clone https://github.com/anibali/torchvid.git /tmp/torchvid
cd /tmp/torchvid
luarocks make rockspecs/torchvid-scm-0.rockspec
rm -rf /tmp/torchvid
```
