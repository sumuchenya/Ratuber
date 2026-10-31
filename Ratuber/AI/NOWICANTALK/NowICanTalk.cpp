#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <mmsystem.h>

#include "NowICanTalk.h"

#include <chrono>
#include <thread>
#include <vector>

#include "../AIPlatform.h"
#include "../AIProtocol.h"

#pragma comment(lib, "winmm.lib")

namespace Ratuber::AI
{
    namespace
    {
        constexpr const char* kTag = "NowICanTalk";

        using Clock = std::chrono::steady_clock;

        // 一个小句在本地的状态。
        struct Segment
        {
            int index = 0;

            int count = 0;

            std::string text;

            std::string filename;

            Protocol::SegmentState state = Protocol::SegmentState::Queued;
        };

        enum class WaitResult
        {
            // 已生成，可以播放。
            Ready,

            // 这个小句生成失败，跳过。
            Failed,

            // 等太久，跳过。
            Timeout,

            // 整段都没法继续（服务器没了、任务不存在、被要求停止）。
            Abort,
        };
    }


    NowICanTalk::NowICanTalk(TalkConfig config)
        :
        m_config(std::move(config))
    {}

    void NowICanTalk::Stop()
    {
        m_stop = true;

        // 打断正在同步播放的声音。
        PlaySoundW(nullptr, nullptr, 0);
    }

    void NowICanTalk::Interrupt()
    {
        m_interrupt = true;

        PlaySoundW(nullptr, nullptr, 0);
    }

    void NowICanTalk::ClearInterrupt()
    {
        m_interrupt = false;
    }

    void NowICanTalk::Discard(
        const std::string& ttsResponseJson,
        const std::atomic<bool>& stop
    )
    {
        Protocol::TtsSubmit submit;
        std::string error;

        if (!Protocol::ParseTtsSubmitResponse(ttsResponseJson, submit, error))
        {
            return;
        }

        const auto deadline = Clock::now()
            + std::chrono::milliseconds(m_config.segmentTimeoutMs);

        while (!stop.load() && Clock::now() < deadline)
        {
            const Platform::Http::Response response =
                Platform::Http::Get(m_config.ttsPort, "/tts/" + submit.taskId);

            // 任务没了或服务器连不上：不用等了。
            if (!response.Ok())
            {
                break;
            }

            Protocol::TtsStatus status;
            std::string parseError;

            if (!Protocol::ParseTtsStatusResponse(response.body, status, parseError))
            {
                break;
            }

            bool allDone = true;

            for (const Protocol::TtsSegmentStatus& s : status.segments)
            {
                if (s.state == Protocol::SegmentState::Queued
                    || s.state == Protocol::SegmentState::Processing)
                {
                    allDone = false;
                }
            }

            if (allDone)
            {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        for (const Protocol::TtsSegment& segment : submit.segments)
        {
            Platform::DeleteFileWithRetry(
                m_config.tempVoiceDir
                + L"\\" + Platform::Utf8ToWide(segment.filename),
                1
            );
        }

        Platform::Log(kTag, "已丢弃任务 " + submit.taskId + " 的语音文件");
    }


    TalkResult NowICanTalk::Play(
        const std::string& ttsResponseJson,
        const std::function<void()>& onFirstAudioStart,
        const SegmentCallback& onSegmentStart
    )
    {
        TalkResult result;

        Protocol::TtsSubmit submit;
        std::string error;

        if (!Protocol::ParseTtsSubmitResponse(
                ttsResponseJson, submit, error))
        {
            result.error = error;

            return result;
        }

        std::vector<Segment> segments;

        for (const Protocol::TtsSegment& item : submit.segments)
        {
            Segment segment;

            segment.index = item.index;
            segment.count = item.count > 0
                ? item.count
                : static_cast<int>(submit.segments.size());
            segment.text = item.text;
            segment.filename = item.filename;

            segments.push_back(std::move(segment));
        }

        result.total = static_cast<int>(segments.size());

        Platform::Log(kTag, "任务 " + submit.taskId + "：共 "
            + std::to_string(result.total) + " 个小句");

        // ----------------------------------------------------
        // 等待某个小句生成完毕
        // ----------------------------------------------------

        auto waitForSegment = [&](Segment& target, std::string& waitError)
            -> WaitResult
        {
            const auto deadline = Clock::now()
                + std::chrono::milliseconds(m_config.segmentTimeoutMs);

            int consecutiveErrors = 0;

            for (;;)
            {
                if (m_stop || m_interrupt)
                {
                    waitError = "已停止";

                    return WaitResult::Abort;
                }

                if (target.state == Protocol::SegmentState::Completed)
                {
                    return WaitResult::Ready;
                }

                if (target.state == Protocol::SegmentState::Failed)
                {
                    return WaitResult::Failed;
                }

                const Platform::Http::Response response =
                    Platform::Http::Get(
                        m_config.ttsPort,
                        "/tts/" + submit.taskId
                    );

                if (!response.Ok())
                {
                    // 任务不存在：服务器重启过，后面的小句都不会有了。
                    if (response.connected && response.status == 404)
                    {
                        waitError = "TTS 任务已经不存在"
                            "（tts_server 是不是重启过？）";

                        return WaitResult::Abort;
                    }

                    if (++consecutiveErrors >= 5)
                    {
                        waitError = "查询 TTS 状态失败："
                            + Platform::Http::Describe(response);

                        return WaitResult::Abort;
                    }
                }
                else
                {
                    consecutiveErrors = 0;

                    Protocol::TtsStatus status;
                    std::string parseError;

                    if (Protocol::ParseTtsStatusResponse(
                            response.body, status, parseError))
                    {
                        for (const Protocol::TtsSegmentStatus& s
                            : status.segments)
                        {
                            for (Segment& segment : segments)
                            {
                                if (segment.index == s.index)
                                {
                                    segment.state = s.state;

                                    if (!s.filename.empty())
                                    {
                                        segment.filename = s.filename;
                                    }
                                }
                            }
                        }

                        // 刚更新完，马上判断一次，不用多睡一轮。
                        if (target.state == Protocol::SegmentState::Completed)
                        {
                            return WaitResult::Ready;
                        }

                        if (target.state == Protocol::SegmentState::Failed)
                        {
                            return WaitResult::Failed;
                        }
                    }
                }

                if (Clock::now() >= deadline)
                {
                    waitError = "等第 " + std::to_string(target.index)
                        + " 个小句生成超时";

                    return WaitResult::Timeout;
                }

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(m_config.pollIntervalMs)
                );
            }
        };

        // ----------------------------------------------------
        // 严格按 segment_index 从小到大播放
        // ----------------------------------------------------

        bool firstAudio = true;

        for (Segment& segment : segments)
        {
            if (m_stop || m_interrupt)
            {
                break;
            }

            std::string waitError;

            const WaitResult wait = waitForSegment(segment, waitError);

            if (wait == WaitResult::Abort)
            {
                // 被跳过打断不算错误。
                if (!m_interrupt)
                {
                    result.error = waitError;
                }

                break;
            }

            if (wait != WaitResult::Ready)
            {
                const std::string reason = wait == WaitResult::Failed
                    ? "第 " + std::to_string(segment.index) + " 个小句生成失败"
                    : waitError;

                Platform::Log(kTag, reason + "，跳过");

                if (result.error.empty())
                {
                    result.error = reason;
                }

                continue;
            }

            const std::wstring path = m_config.tempVoiceDir
                + L"\\" + Platform::Utf8ToWide(segment.filename);

            if (!Platform::FileExists(path))
            {
                result.error = "找不到 WAV 文件："
                    + Platform::WideToUtf8(path)
                    + "（tempvoice 目录是不是和 tts_server.py 的不一致？）";

                break;
            }

            // 声音和动作 / 表情同时开始。
            if (firstAudio)
            {
                firstAudio = false;

                if (onFirstAudioStart)
                {
                    onFirstAudioStart();
                }
            }

            if (onSegmentStart)
            {
                onSegmentStart(segment.index, segment.count, segment.text);
            }

            m_speaking = true;

            // 同步播放：这一句播完才会返回，所以不可能乱序。
            const BOOL played = PlaySoundW(
                path.c_str(),
                nullptr,
                SND_FILENAME | SND_SYNC | SND_NODEFAULT
            );

            m_speaking = false;

            if (played)
            {
                ++result.played;
            }
            else if (!m_stop && !m_interrupt)
            {
                const std::string reason = "第 "
                    + std::to_string(segment.index)
                    + " 个小句播放失败（PlaySound）";

                Platform::Log(kTag, reason);

                if (result.error.empty())
                {
                    result.error = reason;
                }
            }

            // 播完就删。
            Platform::DeleteFileWithRetry(path);
        }

        m_speaking = false;

        // 没播到的（失败、中止）也一并清掉。
        for (const Segment& segment : segments)
        {
            Platform::DeleteFileWithRetry(
                m_config.tempVoiceDir
                + L"\\" + Platform::Utf8ToWide(segment.filename),
                1
            );
        }

        result.interrupted = m_interrupt.load();

        result.ok = result.played == result.total;

        if (!result.ok && result.error.empty() && !result.interrupted)
        {
            result.error = "只播放了 " + std::to_string(result.played)
                + "/" + std::to_string(result.total) + " 个小句";
        }

        Platform::Log(kTag, "任务 " + submit.taskId + " 播放结束："
            + std::to_string(result.played) + "/"
            + std::to_string(result.total));

        return result;
    }
}
