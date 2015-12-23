local torchvid = require('torchvid')

describe('torchvid', function()
  describe('Video', function()
    local video
    local n_video_frames = 418

    before_each(function()
      video = torchvid.Video.new('./test/data/centaur_1.mpg')
    end)

    describe(':duration', function()
      it('should return the approximate video duration', function()
        local expected = 14.0754
        local actual = video:duration()
        assert.is_near(expected, actual, 0.5)
      end)
    end)

    describe(':next_video_frame', function()
      it('should read a video frame', function()
        local frame = video:next_video_frame()
        assert.is_not_nil(frame)
      end)

      it('should return error after end of stream is reached', function()
        local ok = true
        for i=0,n_video_frames do
          assert.is_truthy(ok)
          ok = pcall(video.next_video_frame, video)
        end
        assert.is_falsy(ok)
      end)
    end)
  end)

  describe('VideoFrame', function()
    local video
    local n_video_frames = 418

    before_each(function()
      video = torchvid.Video.new('./test/data/centaur_1.mpg')
    end)

    describe(':to_byte_tensor', function()
      it('should return a ByteTensor of the correct dimensions', function()
        local frame = video:next_video_frame()
        local tensor = frame:to_byte_tensor()
        assert.are.same(torch.typename(tensor), 'torch.ByteTensor')
        assert.are.same(tensor:size():totable(), {3, 240, 320})
      end)
    end)
  end)
end)
