// ============================================================
// TalkSmokeTest.cpp
//
// 第 2 份的冒烟测试（只测声音，不需要界面）：
//   假 AI 回复 -> NowICanListen -> tts_server -> NowICanTalk 按序播放 -> 删除 WAV
//
// 运行前先开 ai_server.py（8001）和 tts_server.py（8000）。
//
// 用法：
//   TalkSmokeTest                      默认 tempvoice 目录 D:\Ratuber\Ratuber\tempvoice
//   TalkSmokeTest <tempvoice目录>      指定目录
//
// 验证点：
//   1. 长句会被切成多段，听到的顺序必须和文字顺序一致；
//   2. 播完后 tempvoice 里对应的 WAV 被删掉；
//   3. 播放期间日志里 speaking=true，播完 speaking=false。
//
// NowICanPlay 需要 Live2D 的 MotionManager，这里不测，留给第 3 份整合时一起测。
// ============================================================

#include <string>

#include "../AIPlatform.h"
#include "../NOWICANLISTEN/NowICanListen.h"
#include "../NOWICANTALK/NowICanTalk.h"

namespace
{
    using Ratuber::AI::Platform::Log;

    class FakeChat : public Ratuber::AI::API::IChatAPI
    {
    public:
        bool SendMessage(const std::string&, std::string& reply, std::string&) override
        {
            reply =
                R"({"text":"第一句话是你好呀。第二句话是今天天气真不错。第三句话是我们一起加油吧！","emotion":"happy",)"
                R"("max_level":"compose","client_state":{"busy":false}})";
            return true;
        }
    };
}

int main(int argc, char** argv)
{
    Ratuber::AI::TalkConfig config;
    config.tempVoiceDir = argc > 1
        ? Ratuber::AI::Platform::Utf8ToWide(argv[1])
        : L"D:\\Ratuber\\Ratuber\\tempvoice";

    FakeChat chat;
    Ratuber::AI::NowICanListen listen;
    listen.SetChatAPI(&chat);

    const auto result = listen.Process(
        R"({"status":"ok","text":"测试"})", []() { return false; });

    for (const auto& e : result.errors)
        Log("错误 " + e.source, e.message);

    if (!result.hasSpeech)
    {
        Log("结果", "没有拿到 TTS 输出");
        return 1;
    }

    Ratuber::AI::NowICanTalk talk(config);

    const auto talkResult = talk.Play(
        result.ttsResponseJson,
        []() { Log("Talk", "第一个小句开始播放（此刻动作/表情应同时开始）"); });

    Log("结果", "播放 " + std::to_string(talkResult.played) + "/"
        + std::to_string(talkResult.total)
        + (talkResult.error.empty() ? "" : "，问题：" + talkResult.error));
    Log("结果", std::string("speaking=") + (talk.IsSpeaking() ? "true" : "false"));

    return talkResult.ok ? 0 : 1;
}
