/***
@module torchvid
*/

/* Lua libs */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#if LUA_VERSION_NUM < 502
# define luaL_newlib(L,l) (lua_newtable(L), luaL_register(L,NULL,l))
#endif

void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
  luaL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    int i;
    for (i = 0; i < nup; i++)  /* copy upvalues to the top */
      lua_pushvalue(L, -nup);
    lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    lua_setfield(L, -(nup + 2), l->name);
  }
  lua_pop(L, nup);  /* remove upvalues */
}

/* Torch libs */
#include <luaT.h>
#include <TH/TH.h>

/* FFmpeg libs */
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/avcodec.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>

#include <string.h>

typedef unsigned char byte;

#define TYPE float
#include "pack_as.h"
#undef TYPE

#define TYPE byte
#include "pack_as.h"
#undef TYPE

typedef enum {
  TVError_None = 0,
  TVError_EOF,
  TVError_ReadFail,
  TVError_DecodeFail,
  TVError_FilterFail
} TVError;

/***
@type ImageFrame
*/
typedef struct {
  AVFrame *frame;
  float timestamp;
} ImageFrame;

static int calculate_tensor_channels(AVFrame *frame) {
  const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(frame->format);
  int is_packed_rgb = (desc->flags & (AV_PIX_FMT_FLAG_PLANAR | AV_PIX_FMT_FLAG_RGB)) == AV_PIX_FMT_FLAG_RGB;

  int n_channels = 3;

  if(!is_packed_rgb) {
    // Calculate number of channels (eg 3 for YUV, 1 for greyscale)
    for(n_channels = 4;
      n_channels > 0 && frame->linesize[n_channels - 1] == 0;
      --n_channels);
  }

  return n_channels;
}

/***
Copies video frame pixel data into a `torch.ByteTensor`.

@function to_byte_tensor
@treturn torch.ByteTensor A tensor representation of the pixel data.
*/
static int ImageFrame_to_byte_tensor(lua_State *L) {
  ImageFrame *self = (ImageFrame*)luaL_checkudata(L, 1, "ImageFrame");

  int n_channels = calculate_tensor_channels(self->frame);
  THByteTensor *tensor = THByteTensor_newWithSize3d(
    n_channels, self->frame->height, self->frame->width);

  if(pack_any_as_byte(tensor->storage->data, self->frame) < 0) {
    THByteTensor_free(tensor);
    return luaL_error(L, "unsupported pixel format");
  }

  luaT_pushudata(L, tensor, "torch.ByteTensor");

  return 1;
}

/***
Copies video frame pixel data into a `torch.FloatTensor`.

For most pixel formats, this means that all channels will contain values between
0 and 1. If the pixel format is YUV, the chroma channels will contain values
between -1 and 1.

@function to_float_tensor
@treturn torch.FloatTensor A tensor representation of the pixel data.
*/
static int ImageFrame_to_float_tensor(lua_State *L) {
  ImageFrame *self = (ImageFrame*)luaL_checkudata(L, 1, "ImageFrame");

  int n_channels = calculate_tensor_channels(self->frame);
  THFloatTensor *tensor = THFloatTensor_newWithSize3d(
    n_channels, self->frame->height, self->frame->width);

  if(pack_any_as_float(tensor->storage->data, self->frame) < 0) {
    THFloatTensor_free(tensor);
    return luaL_error(L, "unsupported pixel format");
  }

  const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(self->frame->format);
  int is_yuv = !(desc->flags & PIX_FMT_RGB) && desc->nb_components >= 2;

  if(is_yuv) {
    THFloatTensor *tensor_y = THFloatTensor_newSelect(tensor, 0, 0);
    THFloatTensor_div(tensor_y, tensor_y, 255);
    THFloatTensor_free(tensor_y);

    THFloatTensor *tensor_u = THFloatTensor_newSelect(tensor, 0, 1);
    THFloatTensor_div(tensor_u, tensor_u, 128);
    THFloatTensor_add(tensor_u, tensor_u, -1);
    THFloatTensor_free(tensor_u);

    THFloatTensor *tensor_v = THFloatTensor_newSelect(tensor, 0, 2);
    THFloatTensor_div(tensor_v, tensor_v, 128);
    THFloatTensor_add(tensor_v, tensor_v, -1);
    THFloatTensor_free(tensor_v);
  } else {
    THFloatTensor_div(tensor, tensor, 255);
  }

  luaT_pushudata(L, tensor, "torch.FloatTensor");

  return 1;
}

/***
Get the timestamp of this image frame (in seconds).

@function timestamp
@treturn number The timestamp (in seconds).
*/
static int ImageFrame_timestamp(lua_State *L) {
  ImageFrame *self = (ImageFrame*)luaL_checkudata(L, 1, "ImageFrame");

  lua_pushnumber(L, self->timestamp);

  return 1;
}

static const luaL_Reg ImageFrame_functions[] = {
  {NULL, NULL}
};

static const luaL_Reg ImageFrame_methods[] = {
  {"to_byte_tensor", ImageFrame_to_byte_tensor},
  {"to_float_tensor", ImageFrame_to_float_tensor},
  {"timestamp", ImageFrame_timestamp},
  {NULL, NULL}
};

static void register_ImageFrame(lua_State *L, int m) {
  luaL_newmetatable(L, "ImageFrame");
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");

  luaL_setfuncs(L, ImageFrame_methods, 0);
  lua_pop(L, 1);

  luaL_newlib(L, ImageFrame_functions);

  lua_setfield(L, m, "ImageFrame");
}

/***
@type Video
*/
typedef struct {
  int skip_destroy;
  AVFormatContext *format_context;
  int video_stream_index;
  AVCodecContext *image_decoder_context;
  AVPacket packet;
  AVFrame *frame;
  AVFilterGraph *filter_graph;
  AVFilterContext *buffersrc_context;
  AVFilterContext *buffersink_context;
  AVFrame *filtered_frame;
  int64_t seek_pts;
} Video;

/***
Creates a new Video.

@function Video.new
@string path Absolute or relative path to a video file.
@treturn Video
*/
static int Video_new(lua_State *L) {
  if(lua_gettop(L) != 1) {
    return luaL_error(L, "invalid number of arguments: <path> expected");
  }

  const char *path = luaL_checkstring(L, 1);

  Video *self = lua_newuserdata(L, sizeof(Video));
  memset(self, 0, sizeof(Video));

  if(avformat_open_input(&self->format_context, path, NULL, NULL) < 0) {
    return luaL_error(L, "failed to open video input for %s", path);
  }

  if(avformat_find_stream_info(self->format_context, NULL) < 0) {
    return luaL_error(L, "failed to find stream info for %s", path);
  }

  AVCodec *decoder;
  self->video_stream_index = av_find_best_stream(self->format_context,
    AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
  if(self->video_stream_index < 0) {
    return luaL_error(L, "failed to find video stream for %s", path);
  }

  self->image_decoder_context = self->format_context->streams[self->video_stream_index]->codec;
  av_opt_set_int(self->image_decoder_context, "refcounted_frames", 1, 0);

  if(avcodec_open2(self->image_decoder_context, decoder, NULL) < 0) {
    return luaL_error(L, "failed to open video decoder for %s", path);
  }

  // av_dump_format(self->format_context, 0, path, 0);

  self->frame = av_frame_alloc();

  self->seek_pts = AV_NOPTS_VALUE;

  luaL_getmetatable(L, "Video");
  lua_setmetatable(L, -2);

  return 1;
}

/***
Get the duration of the video in seconds.

@function duration
@treturn number The duration of the video in seconds.
*/
static int Video_duration(lua_State *L) {
  Video *self = (Video*)luaL_checkudata(L, 1, "Video");

  lua_Number duration_in_seconds =
    (lua_Number)self->format_context->duration / AV_TIME_BASE;

  lua_pushnumber(L, duration_in_seconds);

  return 1;
}

/***
Guess the average frame rate of the video in FPS.

@function guess_image_frame_rate
@treturn number The frame rate, or zero if unknown.
*/
static int Video_guess_image_frame_rate(lua_State *L) {
  Video *self = (Video*)luaL_checkudata(L, 1, "Video");

  AVRational framerate = av_guess_frame_rate(
    self->format_context,
    self->format_context->streams[self->video_stream_index],
    NULL);

  lua_pushnumber(L, (lua_Number)framerate.num / framerate.den);

  return 1;
}

/***
Get the number of image frames in the video.

@function get_image_frame_count
@treturn number The number of frames, or zero if unknown.
*/
static int Video_get_image_frame_count(lua_State *L) {
  Video *self = (Video*)luaL_checkudata(L, 1, "Video");

  AVStream *image_stream =
    self->format_context->streams[self->video_stream_index];

  lua_pushnumber(L, (lua_Number)image_stream->nb_frames);

  return 1;
}

/***
Apply a filterchain to the video.

@function filter
@string pixel_format_name The desired output pixel format.
@string[opt='null'] filterchain A description of the filterchain.
@treturn number The duration of the video in seconds.
*/
static int Video_filter(lua_State *L) {
  int n_args = lua_gettop(L);

  Video *self = (Video*)luaL_checkudata(L, 1, "Video");
  const char *pixel_format_name = luaL_checkstring(L, 2);
  const char *filterchain;
  if(n_args < 3) {
    filterchain = "null";
  } else {
    filterchain = luaL_checkstring(L, 3);
  }

  if(self->filter_graph) {
    return luaL_error(L, "filter already set for this video");
  }

  const char* error_msg = 0;

  AVFilter *buffersrc = avfilter_get_by_name("buffer");
  AVFilter *buffersink = avfilter_get_by_name("buffersink");
  AVFilterInOut *outputs = avfilter_inout_alloc();
  AVFilterInOut *inputs = avfilter_inout_alloc();
  AVFilterGraph *filter_graph = avfilter_graph_alloc();

  char in_args[512];
  snprintf(in_args, sizeof(in_args),
    "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
    self->image_decoder_context->width,
    self->image_decoder_context->height,
    self->image_decoder_context->pix_fmt,
    self->image_decoder_context->time_base.num,
    self->image_decoder_context->time_base.den,
    self->image_decoder_context->sample_aspect_ratio.num,
    self->image_decoder_context->sample_aspect_ratio.den);

  AVFilterContext *buffersrc_context;
  AVFilterContext *buffersink_context;

  if(avfilter_graph_create_filter(&buffersrc_context, buffersrc, "in",
    in_args, NULL, filter_graph) < 0)
  {
    error_msg = "cannot create buffer source";
    goto end;
  }

  if(avfilter_graph_create_filter(&buffersink_context, buffersink, "out",
    NULL, NULL, filter_graph) < 0)
  {
    error_msg = "cannot create buffer sink";
    goto end;
  }

  enum AVPixelFormat pix_fmt = av_get_pix_fmt(pixel_format_name);
  if(pix_fmt == AV_PIX_FMT_NONE) {
    error_msg = "invalid pixel format name";
    goto end;
  }
  if(av_opt_set_bin(buffersink_context, "pix_fmts",
    (const unsigned char*)&pix_fmt, sizeof(enum AVPixelFormat),
    AV_OPT_SEARCH_CHILDREN) < 0)
  {
    error_msg = "failed to set output pixel format";
    goto end;
  }

  outputs->name       = av_strdup("in");
  outputs->filter_ctx = buffersrc_context;
  outputs->pad_idx    = 0;
  outputs->next       = NULL;

  inputs->name       = av_strdup("out");
  inputs->filter_ctx = buffersink_context;
  inputs->pad_idx    = 0;
  inputs->next       = NULL;

  if(avfilter_graph_parse_ptr(filter_graph, filterchain,
    &inputs, &outputs, NULL) < 0)
  {
    error_msg = "failed to parse filterchain description";
    goto end;
  }

  if(avfilter_graph_config(filter_graph, NULL) < 0) {
    error_msg = "failed to configure filter graph";
    goto end;
  }

  // Copy self
  Video *filtered_video = lua_newuserdata(L, sizeof(Video));
  *filtered_video = *self;
  self->skip_destroy = 1;

  filtered_video->filter_graph = filter_graph;
  filtered_video->buffersrc_context = buffersrc_context;
  filtered_video->buffersink_context = buffersink_context;
  filtered_video->filtered_frame = av_frame_alloc();

  luaL_getmetatable(L, "Video");
  lua_setmetatable(L, -2);

end:
  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);

  if(error_msg) return luaL_error(L, error_msg);

  return 1;
}

static TVError read_image_frame(Video *self, ImageFrame *video_frame, int decode_only, int filter_only) {
  if(self->filter_graph && av_buffersink_get_frame(self->buffersink_context,
    self->filtered_frame) >= 0)
  {
    video_frame->frame = self->filtered_frame;
  } else {
    int found_video_frame;

    if(filter_only) {
      goto start_filter_video_frame;
    }

start_read_video_frame:
    found_video_frame = 0;

    while(!found_video_frame) {
      // Clear the packet
      av_packet_unref(&self->packet);

      int errnum = av_read_frame(self->format_context, &self->packet);
      if(errnum == AVERROR(EAGAIN)) {
        continue;
      } else if(errnum == AVERROR_EOF) {
        // We've exhausted av_read_frame, it's time to milk
        // avcodec_decode_video2
        av_frame_unref(self->frame);

        if(avcodec_decode_video2(self->image_decoder_context, self->frame,
          &found_video_frame, &self->packet) < 0)
        {
          return TVError_DecodeFail;
        }

        if(!found_video_frame) {
          return TVError_EOF;
        }

        break;
      } else if(errnum != 0) {
        return TVError_ReadFail;
      }

      if(self->packet.stream_index == self->video_stream_index) {
        av_frame_unref(self->frame);

        if(avcodec_decode_video2(self->image_decoder_context, self->frame,
          &found_video_frame, &self->packet) < 0)
        {
          return TVError_DecodeFail;
        }
      }
    }

start_filter_video_frame:
    if(!decode_only && self->filter_graph) {
      // Push the decoded frame into the filtergraph
      if(av_buffersrc_add_frame_flags(self->buffersrc_context,
        self->frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
      {
        return TVError_FilterFail;
      }

      // Pull filtered frames from the filtergraph
      av_frame_unref(self->filtered_frame);
      if(av_buffersink_get_frame(self->buffersink_context, self->filtered_frame) < 0) {
        goto start_read_video_frame;
      }
      video_frame->frame = self->filtered_frame;
    } else {
      video_frame->frame = self->frame;
    }
  }

  return TVError_None;
}

/***
Read the next video frame from the video.

@function next_image_frame
@treturn ImageFrame
*/
static int Video_next_image_frame(lua_State *L) {
  Video *self = (Video*)luaL_checkudata(L, 1, "Video");

  ImageFrame *video_frame = lua_newuserdata(L, sizeof(ImageFrame));

  TVError err;

  if(self->seek_pts != AV_NOPTS_VALUE) {
    // Do fine-grained seek
    err = read_image_frame(self, video_frame, 1, 0);
    while(err == TVError_None) {
      int64_t pts = av_frame_get_best_effort_timestamp(video_frame->frame);
      if(pts != AV_NOPTS_VALUE && pts >= self->seek_pts) {
        break;
      }
      err = read_image_frame(self, video_frame, 1, 0);
    }
    self->seek_pts = AV_NOPTS_VALUE;
    if(err == TVError_None) {
      err = read_image_frame(self, video_frame, 0, 1);
    }
  } else {
    err = read_image_frame(self, video_frame, 0, 0);
  }

  switch(err) {
    case TVError_EOF:
      return luaL_error(L, "reached end of video");
    case TVError_ReadFail:
      return luaL_error(L, "couldn't read next frame");
    case TVError_DecodeFail:
      return luaL_error(L, "couldn't decode video frame");
    case TVError_FilterFail:
      return luaL_error(L, "error while feeding the filtergraph");
  }

  float time_base = av_q2d(self->format_context->streams[self->video_stream_index]->time_base);
  video_frame->timestamp = av_frame_get_best_effort_timestamp(video_frame->frame) * time_base;

  luaL_getmetatable(L, "ImageFrame");
  lua_setmetatable(L, -2);

  return 1;
}

/***
Seek to the first keyframe before the frame number specified.

@function seek
@number seek_target The position to seek to (in seconds).
@treturn Video This video object.
*/
static int Video_seek(lua_State *L) {
  Video *self = (Video*)luaL_checkudata(L, 1, "Video");
  lua_Number seek_target = luaL_checknumber(L, 2);

  float time_base = av_q2d(self->format_context->streams[self->video_stream_index]->time_base);
  int64_t timestamp = (int64_t)floor(seek_target / time_base);

  // Do course seek to keyframe
  if(av_seek_frame(self->format_context, self->video_stream_index, timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
    return luaL_error(L, "error while seeking");
  } else {
    avcodec_flush_buffers(self->image_decoder_context);
  }

  // Set seek_pts so fine-grained seek can happen when the next frame is read
  self->seek_pts = timestamp;

  lua_pop(L, 1);

  return 1;
}

static int Video_destroy(lua_State *L) {
  Video *self = (Video*)luaL_checkudata(L, 1, "Video");

  if(!self->skip_destroy) {
    avcodec_close(self->image_decoder_context);
    avformat_close_input(&self->format_context);

    av_packet_unref(&self->packet);

    if(self->frame != NULL) {
      av_frame_unref(self->frame);
      av_frame_free(&self->frame);
    }

    if(self->filter_graph) {
      avfilter_graph_free(&self->filter_graph);
    }

    if(self->filtered_frame != NULL) {
      av_frame_unref(self->filtered_frame);
      av_frame_free(&self->filtered_frame);
    }
  }

  return 0;
}

static const luaL_Reg Video_functions[] = {
  {"new", Video_new},
  {NULL, NULL}
};

static const luaL_Reg Video_methods[] = {
  {"duration", Video_duration},
  {"guess_image_frame_rate", Video_guess_image_frame_rate},
  {"get_image_frame_count", Video_get_image_frame_count},
  {"filter", Video_filter},
  {"next_image_frame", Video_next_image_frame},
  {"seek", Video_seek},
  {"__gc", Video_destroy},
  {NULL, NULL}
};

static void register_Video(lua_State *L, int m) {
  luaL_newmetatable(L, "Video");
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");

  luaL_setfuncs(L, Video_methods, 0);
  lua_pop(L, 1);

  luaL_newlib(L, Video_functions);

  lua_setfield(L, m, "Video");
}

int luaopen_torchvid(lua_State *L) {
  // Initialization
  av_log_set_level(AV_LOG_ERROR);
  avcodec_register_all();
  av_register_all();
  avfilter_register_all();

  // Create table for module
  lua_newtable(L);
  int m = lua_gettop(L);

  // Add values for classes
  register_Video(L, m);
  register_ImageFrame(L, m);

  return 1;
}
