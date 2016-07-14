require('torch')
local torchvid = require('torchvid')

describe('torchvid', function()
  describe('Video', function()
    local video
    local n_video_frames = 419

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

    describe(':filter', function()
      it('should return a new instance of Video', function()
        local filtered_video = video:filter('rgb24', 'hflip')
        assert.not_same(video, filtered_video)
      end)
    end)

    describe(':next_image_frame', function()
      it('should read a video frame', function()
        local frame = video:next_image_frame()
        assert.is_not_nil(frame)
      end)

      it('should return error after end of stream is reached', function()
        local ok = true
        for i=0,n_video_frames do
          assert.is_truthy(ok)
          ok = pcall(video.next_image_frame, video)
        end
        assert.is_falsy(ok)
      end)
    end)

    describe(':guess_image_frame_rate', function()
      it('should return the correct average frame rate', function()
        assert.is_near(30, video:guess_image_frame_rate(), 0.1)
      end)
    end)

    describe(':seek', function()
      it('should return the same Video', function()
        assert.is_same(video, video:seek(1.0))
      end)

      it('should change the position of the video stream', function()
        video:seek(13.5)
        -- Expect to reach end of video within 30 frames
        local ok = true
        local i = 0
        while ok and i < 30 do
          ok = pcall(video.next_image_frame, video)
          i = i + 1
        end
        assert.is_falsy(ok)
      end)
    end)
  end)

  describe('ImageFrame', function()
    local video

    before_each(function()
      video = torchvid.Video.new('./test/data/centaur_1.mpg')
    end)

    describe(':timestamp', function()
      it('should return the timestamp of the frame in seconds', function()
        for i=1,29 do
          video:next_image_frame()
        end
        local frame = video:next_image_frame()
        assert.is_near(1.0, frame:timestamp(), 0.01)
      end)
    end)

    describe(':to_byte_tensor', function()
      it('should return a ByteTensor of the correct dimensions', function()
        local frame = video:next_image_frame()
        local tensor = frame:to_byte_tensor()
        assert.are.same('torch.ByteTensor', torch.typename(tensor))
        assert.are.same({3, 240, 320}, tensor:size():totable())
      end)

      it('should handle packed RGB24', function()
        local frame = video:filter('rgb24'):next_image_frame()
        local tensor = frame:to_byte_tensor()
        assert.are.same('torch.ByteTensor', torch.typename(tensor))
        assert.are.same({3, 240, 320}, tensor:size():totable())
      end)

      it('should convert greyscale frame into a 1-channel byte tensor', function()
        -- 3x3 pixel greyscale version of first frame
        local expected = {{
          {0, 1, 0},
          {0, 24, 1},
          {0, 18, 0}
        }}

        local actual = video
          :filter('gray', 'scale=3:3')
          :next_image_frame()
          :to_byte_tensor()
          :totable()

        assert.are.same(expected, actual)
      end)
    end)

    describe(':to_float_tensor', function()
      it('should return correct YUV FloatTensor', function()
        local expected = {{
          { 0.0392,  0.0745,  0.0392},
          { 0.0039,  0.1451,  0.0667},
          { 0.0039,  0.1216,  0.0549}
        }, {
          { 0.0234,  0.0078,  0.0234},
          { 0.0156, -0.0156,  0.0078},
          { 0.0234, -0.0156,  0.0000}
        }, {
          { 0.0000,  0.0156,  0.0078},
          {-0.0078,  0.0156,  0.0078},
          {-0.0078,  0.0156,  0.0078}
        }}

        local actual = video
          :filter('yuv444p', 'scale=3:3')
          :next_image_frame()
          :to_float_tensor()
          :totable()

        for i=1,3 do
          for j=1,3 do
            for k=1,3 do
              assert.is_near(expected[i][j][k], actual[i][j][k], 0.0001)
            end
          end
        end
      end)
    end)
  end)
end)
