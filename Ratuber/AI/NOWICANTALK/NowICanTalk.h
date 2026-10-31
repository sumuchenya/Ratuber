#pragma once

#include <atomic>
#include <functional>
#include <string>

// ============================================================
// NowICanTalk.h
//
// 职责：把 tts_server.py 生成出来的音频，按顺序播放。
//
// 输入是 tts_server.py 的输出（POST /tts 的响应），里面有
// task_id 和每个小句的 segment_index、filename。
//
// 为什么要“按序”：
//   小句短的可能比前面长的先生成出来。
//   这里严格按 segment_index 从小到大播放，
//   第 N 个没生成好就等，绝不抢先播第 N+1 个。
//
// 每个小句播完立刻删除对应的 WAV 文件。
//
// 嘴巴开合：
//   播放某个 WAV 的期间 IsSpeaking() 为 true，
//   AIManager 在这段时间里每帧调用 mouth。播完就不再调用。
// ============================================================

namespace Ratuber::AI
{
    struct TalkConfig
    {
        int ttsPort = 8000;

        // tts_server.py 存放 WAV 的目录，例如 D:\Ratuber\Ratuber\tempvoice
        std::wstring tempVoiceDir;

        // 等一个小句生成的最长时间。
        int segmentTimeoutMs = 120000;

        // 查询生成状态的间隔。
        int pollIntervalMs = 50;
    };

    struct TalkResult
    {
        // 所有小句都播放成功。
        bool ok = false;

        int played = 0;

        int total = 0;

        // 被 Interrupt()（跳过）打断了。
        bool interrupted = false;

        std::string error;
    };

    class NowICanTalk
    {
    public:
        explicit NowICanTalk(
            TalkConfig config = TalkConfig()
        );

        // 按序播放一整段话。阻塞到播完为止，请在工作线程里调用。
        //
        // onFirstAudioStart：
        //   第一个小句即将开始播放的那一刻调用（只调用一次），
        //   AIManager 用它让动作 / 表情和声音同时开始。
        //
        // onSegmentStart：
        //   每个小句即将开始播放时调用（index 从 1 开始），
        //   界面用它让文字随声音一句一句地显现。
        using SegmentCallback = std::function<void(
            int index, int count, const std::string& text)>;

        TalkResult Play(
            const std::string& ttsResponseJson,
            const std::function<void()>& onFirstAudioStart = nullptr,
            const SegmentCallback& onSegmentStart = nullptr
        );

        // 打断当前这一段话（用户点了“跳过”）：
        // 正在播放的小句立刻停下，剩下的不再播，Play 随即返回。
        // 之后要播下一段话之前，必须先调用 ClearInterrupt()。
        void Interrupt();

        void ClearInterrupt();

        // 丢弃一段不会再播放的 TTS 输出（用户取消了任务）：
        // 等 tts_server 把这些小句生成完（它是 FIFO，没法中途取消），然后把 WAV 删掉。
        // 会阻塞，请在后台线程里调用。stop 变为 true 时不再等待，只删已有的文件。
        void Discard(
            const std::string& ttsResponseJson,
            const std::atomic<bool>& stop
        );

        // 是否正在出声（任何线程都可以调用）。
        bool IsSpeaking() const
        {
            return m_speaking.load();
        }

        // 打断当前播放，并让之后的 Play 立刻返回。
        // 只在程序退出时使用。
        void Stop();

    private:
        TalkConfig m_config;

        std::atomic<bool> m_speaking{ false };

        std::atomic<bool> m_stop{ false };

        std::atomic<bool> m_interrupt{ false };
    };
}
