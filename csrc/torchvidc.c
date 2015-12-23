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
#include <libavfilter/buffersink.h>

#include <string.h>

typedef struct {
  AVFrame *frame;
} VideoFrame;

static const luaL_Reg VideoFrame_functions[] = {
  {NULL, NULL}
};

static const luaL_Reg VideoFrame_methods[] = {
  {NULL, NULL}
};

static void register_VideoFrame(lua_State *L, int m) {
  luaL_newmetatable(L, "VideoFrame");
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");

  luaL_setfuncs(L, VideoFrame_methods, 0);
  lua_pop(L, 1);

  luaL_newlib(L, VideoFrame_functions);

  lua_setfield(L, m, "VideoFrame");
}

typedef struct {
  AVFormatContext *format_context;
  int video_stream_index;
  AVCodecContext *video_decoder_context;
  AVPacket packet;
  AVFrame *frame;
} Video;

static int Video_new(lua_State *L) {
  /* Check function argument count */
  if(lua_gettop(L) != 1) {
    return luaL_error(L, "invalid number of arguments: <path> expected");
  }

  /* Pop first argument */
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

  self->video_decoder_context = self->format_context->streams[self->video_stream_index]->codec;

  if(avcodec_open2(self->video_decoder_context, decoder, NULL) < 0) {
    return luaL_error(L, "failed to open video decoder for %s", path);
  }

  // av_dump_format(self->format_context, 0, path, 0);

  self->frame = av_frame_alloc();

  /* Add the metatable to the stack. */
  luaL_getmetatable(L, "Video");
  /* Set the metatable on the userdata. */
  lua_setmetatable(L, -2);

  /* Return number of return values */
  return 1;
}

static int Video_duration(lua_State *L) {
  Video *self = (Video*)luaL_checkudata(L, 1, "Video");

  lua_Number duration_in_seconds =
    (lua_Number)self->format_context->duration / AV_TIME_BASE;

  lua_pushnumber(L, duration_in_seconds);

  return 1;
}

static int Video_next_video_frame(lua_State *L) {
  Video *self = (Video*)luaL_checkudata(L, 1, "Video");

  int found_video_frame = 0;

  while(!found_video_frame) {
    if(av_read_frame(self->format_context, &self->packet) != 0) {
      return luaL_error(L, "couldn't read next frame");
    }

    if(self->packet.stream_index == self->video_stream_index) {
      av_frame_unref(self->frame);

      if(avcodec_decode_video2(self->video_decoder_context, self->frame,
        &found_video_frame, &self->packet) < 0)
      {
        return luaL_error(L, "couldn't decode video frame");
      }
    }
  }

  VideoFrame *video_frame = lua_newuserdata(L, sizeof(VideoFrame));
  video_frame->frame = self->frame;

  luaL_getmetatable(L, "VideoFrame");
  lua_setmetatable(L, -2);

  return 1;
}

static int Video_destroy(lua_State *L) {
  Video *self = (Video*)luaL_checkudata(L, 1, "Video");

  avformat_close_input(&self->format_context);
  avcodec_close(self->video_decoder_context);

  av_packet_unref(&self->packet);

  if(self->frame != NULL) {
    av_frame_unref(self->frame);
    av_frame_free(&self->frame);
  }

  return 0;
}

static const luaL_Reg Video_functions[] = {
  {"new", Video_new},
  {NULL, NULL}
};

static const luaL_Reg Video_methods[] = {
  {"duration", Video_duration},
  {"next_video_frame", Video_next_video_frame},
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

int luaopen_torchvidc(lua_State *L) {
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
  register_VideoFrame(L, m);

  return 1;
}
