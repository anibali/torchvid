#ifndef TYPE
#error Define TYPE before including this file
#endif

#define CONCAT_4_EXPAND(x,y,z,w) x ## y ## z ## w
#define CONCAT_4(x,y,z,w) CONCAT_4_EXPAND(x,y,z,w)

#define pack_(T) CONCAT_4(pack_, T, _as_, TYPE)

static TYPE* pack_(rgb24)(TYPE *dest, AVFrame *frame) {
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

  return dest;
}

static TYPE* pack_(gray8)(TYPE *dest, AVFrame *frame) {
  int y, x;
  int stride = frame->linesize[0];
  unsigned char *channel_data = frame->data[0];
  for(y = 0; y < frame->height; ++y) {
    int offset = stride * y;
    for(x = 0; x < frame->width; ++x) {
      *dest++ = channel_data[offset + x];
    }
  }

  return dest;
}

static TYPE* pack_(yuv444p)(TYPE *dest, AVFrame *frame) {
  // Luma
  dest = pack_(gray8)(dest, frame);

  // Chroma
  int i, y, x;
  for(i = 1; i < 3; ++i) {
    int stride = frame->linesize[i];
    unsigned char *channel_data = frame->data[i];
    for(y = 0; y < frame->height; ++y) {
      int offset = stride * y;
      for(x = 0; x < frame->width; ++x) {
        *dest++ = channel_data[offset + x];
      }
    }
  }

  return dest;
}

static TYPE* pack_(yuv420p)(TYPE *dest, AVFrame *frame) {
  // Luma
  dest = pack_(gray8)(dest, frame);

  // Chroma
  int i, y, x;
  int odd_width = frame->width & 1;
  int half_width = frame->width >> 1;
  for(i = 1; i < 3; ++i) {
    int stride = frame->linesize[i];
    unsigned char *channel_data = frame->data[i];
    for(y = 0; y < frame->height; ++y) {
      int offset = stride * (y >> 1);
      for(x = 0; x < half_width; ++x) {
        *dest++ = channel_data[offset + x];
        *dest++ = channel_data[offset + x];
      }
      if(odd_width) {
        *dest++ = channel_data[offset + x];
      }
    }
  }

  return dest;
}

static TYPE* pack_(yuv422p)(TYPE *dest, AVFrame *frame) {
  // Luma
  dest = pack_(gray8)(dest, frame);

  // Chroma
  int i, y, x;
  int odd_width = frame->width & 1;
  int half_width = frame->width >> 1;
  for(i = 1; i < 3; ++i) {
    int stride = frame->linesize[i];
    unsigned char *channel_data = frame->data[i];
    for(y = 0; y < frame->height; ++y) {
      int offset = stride * y;
      for(x = 0; x < half_width; ++x) {
        *dest++ = channel_data[offset + x];
        *dest++ = channel_data[offset + x];
      }
      if(odd_width) {
        *dest++ = channel_data[offset + x];
      }
    }
  }

  return dest;
}

static int pack_(any)(TYPE *dest, AVFrame *frame) {
  switch(frame->format) {
    case AV_PIX_FMT_RGB24:
      pack_(rgb24)(dest, frame);
      break;
    case AV_PIX_FMT_GRAY8:
      pack_(gray8)(dest, frame);
      break;
    case AV_PIX_FMT_YUV444P:
      pack_(yuv444p)(dest, frame);
      break;
    case AV_PIX_FMT_YUV420P:
      pack_(yuv420p)(dest, frame);
      break;
    case AV_PIX_FMT_YUV422P:
      pack_(yuv422p)(dest, frame);
      break;
    default:
      return -1;
  }

  return 0;
}
