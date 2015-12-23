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
    end)
  end)
end)
