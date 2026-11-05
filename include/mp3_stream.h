#ifndef MP3_STREAM_H
#define MP3_STREAM_H

#include <stdint.h>
#include <stddef.h>

/**
 * @file mp3_stream.h
 * @brief 基于 minimp3 的逐帧 MP3 解码 (输入为内存中的完整文件)
 */

struct Mp3Frame {
    const int16_t* pcm;       // 交错 PCM, 长度 samples * channels
    int            samples;   // 每声道采样数
    int            channels;
    int            hz;
};

class Mp3Stream {
public:
    Mp3Stream();
    ~Mp3Stream();
    Mp3Stream(const Mp3Stream&) = delete;
    Mp3Stream& operator=(const Mp3Stream&) = delete;

    void begin(const uint8_t* data, size_t size);

    // 解码下一帧; 到达结尾返回 false (自动跳过 ID3 等无法解码的数据)
    bool next(Mp3Frame& frame);

private:
    struct Impl;
    Impl* _impl;
};

#endif // MP3_STREAM_H
