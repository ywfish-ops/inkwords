#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "mp3_stream.h"

struct Mp3Stream::Impl {
    mp3dec_t       dec;
    int16_t        pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    const uint8_t* data = nullptr;
    size_t         size = 0;
    size_t         pos = 0;
};

// 解码器状态 + 一帧 PCM 约 11KB, 放堆上避免占用调用方任务栈
Mp3Stream::Mp3Stream() : _impl(new Impl()) {}

Mp3Stream::~Mp3Stream() { delete _impl; }

void Mp3Stream::begin(const uint8_t* data, size_t size) {
    mp3dec_init(&_impl->dec);
    _impl->data = data;
    _impl->size = data ? size : 0;
    _impl->pos = 0;
}

bool Mp3Stream::next(Mp3Frame& frame) {
    Impl& s = *_impl;
    while (s.pos < s.size) {
        mp3dec_frame_info_t info;
        int samples = mp3dec_decode_frame(&s.dec, s.data + s.pos, (int)(s.size - s.pos), s.pcm, &info);
        if (info.frame_bytes == 0) {
            // 剩余数据中再也找不到有效帧
            s.pos = s.size;
            return false;
        }
        s.pos += info.frame_bytes;
        if (samples > 0) {
            frame.pcm = s.pcm;
            frame.samples = samples;
            frame.channels = info.channels;
            frame.hz = info.hz;
            return true;
        }
        // samples == 0 且 frame_bytes > 0: 跳过的 ID3 或无效数据, 继续
    }
    return false;
}
