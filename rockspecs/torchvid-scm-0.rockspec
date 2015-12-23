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
  "torch >= 7.0"
}

build = {
  type = "command",
  build_command = 'cmake -E make_directory build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(LUA_BINDIR)/.." -DCMAKE_INSTALL_PREFIX="$(PREFIX)" && $(MAKE)',
  install_command = "cd build && $(MAKE) install"
}
