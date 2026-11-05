#ifndef WORD_PLAYER_H
#define WORD_PLAYER_H

#include <Arduino.h>
#include "audio_es8311.h"

/**
 * @file word_player.h
 * @brief 后台单词发音: 独立 FreeRTOS 任务从 TF 卡读 MP3、解码并输出, 不阻塞界面
 */

class WordPlayer {
public:
    static WordPlayer& getInstance() {
        static WordPlayer instance;
        return instance;
    }

    // 创建播放任务; audio 须已 begin 成功
    bool begin(AudioEs8311& audio);

    // 请求播放 SPELL_ROOT_DIR/audio/<file>; 正在播放时立即改播新请求 (重按即从头重播)
    // file 为空或播放任务未就绪返回 false; 文件缺失由播放任务在串口提示
    bool play(const char* file);

    // 是否有发音正在播放或排队
    bool isPlaying() const { return _playing || (_queue && uxQueueMessagesWaiting(_queue) > 0); }

private:
    WordPlayer() = default;
    static void taskEntry(void* arg);
    void run();

    AudioEs8311*  _audio = nullptr;
    QueueHandle_t _queue = nullptr;
    volatile bool _playing = false;
};

#endif // WORD_PLAYER_H
