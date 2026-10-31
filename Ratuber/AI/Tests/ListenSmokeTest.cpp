// ============================================================
// ListenSmokeTest.cpp
//
// 第 1 份的冒烟测试：不需要界面、不需要真正的 AI。
//
// 用一个假的 IChatAPI（固定回复一句话），
// 验证 NowICanListen 能不能把回复正确地送给两个服务器：
//   文本            -> tts_server.py   (POST /tts)
//   整段 JSON       -> ai_server.py    (POST /decide)
//
// 运行前先把 ai_server.py（8001）和 tts_server.py（8000）开起来，
// asr_server.py 不需要。
//
// 用法：
//   ListenSmokeTest                普通的一句话
//   ListenSmokeTest busy           模拟“上一个周期还没结束”（busy 应当变成 true）
//   ListenSmokeTest too_long       模拟用户说太久（固定回复“再说一遍嘛”）
//
// 注意：tts_server 会在 tempvoice 里生成 WAV，
// 现在还没有播放模块去删它们，测完可以手动清理。
// ============================================================

#include <string>

#include "../AIPlatform.h"
#include "../AIProtocol.h"
#include "../NOWICANLISTEN/NowICanListen.h"

namespace
{
    using Ratuber::AI::Platform::Log;

    class FakeChat : public Ratuber::AI::API::IChatAPI
    {
    public:
        bool SendMessage(
            const std::string& userMessage,
            std::string& reply,
            std::string& errorMessage
        ) override
        {
            (void)errorMessage;

            Log("FakeChat", "收到：" + userMessage);

            reply =
                R"({"text":"太好了，今天也要加油哦！","emotion":"happy",)"
                R"("max_level":"compose","client_state":{"busy":false}})";

            return true;
        }
    };
}

int main(int argc, char** argv)
{
    bool busy = false;
    bool tooLong = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "busy")
        {
            busy = true;
        }
        else if (arg == "too_long")
        {
            tooLong = true;
        }
    }

    FakeChat chat;

    Ratuber::AI::NowICanListen listen;

    listen.SetChatAPI(&chat);

    const std::string utterance = tooLong
        ? R"({"status":"too_long","text":""})"
        : R"({"status":"ok","text":"今天天气怎么样"})";

    const Ratuber::AI::ListenResult result = listen.Process(
        utterance,
        [busy]() { return busy; }
    );

    Log("结果", std::string("hasSpeech  = ")
        + (result.hasSpeech ? "true" : "false"));

    Log("结果", std::string("hasActions = ")
        + (result.hasActions ? "true" : "false"));

    Log("发给 ai_server 的", result.decideRequestJson);
    Log("tts_server 的输出（给 Talk）", result.ttsResponseJson);
    Log("ai_server 的输出（给 Play）", result.decideResponseJson);

    for (const auto& error : result.errors)
    {
        Log("错误 " + error.source, error.message);
    }

    if (!result.skippedReason.empty())
    {
        Log("跳过", result.skippedReason);
    }

    return result.errors.empty() ? 0 : 1;
}
