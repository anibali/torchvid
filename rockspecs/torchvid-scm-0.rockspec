package = "torchvid"
version = "scm-0"

source = {
  url = "https://github.com/anibali/torchvid/archive/master.zip",
  dir = "torchvid-master"
}

description = {
  summary = "Utilities for loading videos into Torch using FFmpeg libraries",
  homepage = "https://github.com/anibali/torchvid",
  license = "MIT <http://opensource.org/licenses/MIT>"
}

dependencies = {
  "lua >= 5.1"
}

build = {
  type = "builtin",
  modules = {
    ["torchvid"] = "csrc/torchvid.lua",
  }
}
