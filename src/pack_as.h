#ifndef TYPE
#error Define TYPE before including this file
#endif

#define CONCAT_4_EXPAND(x,y,z,w) x ## y ## z ## w
#define CONCAT_4(x,y,z,w) CONCAT_4_EXPAND(x,y,z,w)

#define pack_(T) CONCAT_4(pack_, T, _as_, TYPE)

static void pack_(rgb24)(TYPE *dest, AVFrame *frame) {
  int triple_x_max = frame->width * 3;
  int i, y, triple_x;
  for(i = 0; i < 3; ++i) {
    for(y = 0; y < frame->height; ++y) {
      int offset = y * frame->linesize[0] + i;
      for(triple_x = 0; triple_x < triple_x_max; triple_x += 3) {
        *dest++ = frame->data[0][offset + triple_x];
      }
    }
  }
}

static void pack_(yuv)(TYPE *dest, AVFrame *frame) {
  int n_channels;

  // Calculate number of channels (eg 3 for YUV, 1 for greyscale)
  for(n_channels = 4;
    n_channels > 0 && frame->linesize[n_channels - 1] == 0;
    --n_channels);

  int i, y, x;
  for(i = 0; i < n_channels; ++i) {
    int stride = frame->linesize[i];
    unsigned char *channel_data = frame->data[i];
    for(y = 0; y < frame->height; ++y) {
      int offset = stride * y;
      for(x = 0; x < frame->width; ++x) {
        *dest++ = channel_data[offset + x];
      }
    }
  }
}
