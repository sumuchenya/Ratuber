#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../API/IChatAPI.h"

// ============================================================
// NowICanListen.h
//
// 职责：接收用户说的话，让 AI 回复，再把回复分发给另外两个服务器。
//
//   {"status":"ok","text":"今天天气怎么样"}      <- AIManager 交进来
//        │
//        ▼
//   IChatAPI（让 AI 回复，格式不对时重试一次）
//        │
//        ├── 只把 text ───────────▶ tts_server.py   POST /tts
//        │                              输出 ──▶ NowICanTalk
//        │
//        └── 整段 JSON ───────────▶ ai_server.py    POST /decide
//                                       输出 ──▶ NowICanPlay
//
// 本类不管周期，也不管播放。周期由 AIManager 管，
// “上一个周期有没有结束”通过回调告诉本类，用来决定 busy。
// ============================================================

namespace Ratuber::AI
{
    // 一次处理过程中出的错，source 指明是哪个环节。
    struct ListenError
    {
        // "输入" / "LLM" / "TTS 服务器" / "AI 服务器"
        std::string source;

        std::string message;
    };

    struct ListenResult
    {
        // 有语音可以播。
        bool hasSpeech = false;

        // 有动作 / 表情可以表达。
        bool hasActions = false;

        // AI 的回复文字（已截断到上限）和情绪。LLM 失败时为空。
        // 情绪为空表示这次没有合法情绪，只播语音。
        std::string replyText;
        std::string emotion;

        // 实际发给 ai_server.py 的整段 JSON（busy 已改写，调试用）。
        std::string decideRequestJson;

        // tts_server.py 的输出（POST /tts 的响应）-> NowICanTalk。
        std::string ttsResponseJson;

        // ai_server.py 的输出（POST /decide 的响应）-> NowICanPlay。
        std::string decideResponseJson;

        // 没有可处理内容时的原因（例如用户文本为空），不算错误。
        std::string skippedReason;

        std::vector<ListenError> errors;
    };

    struct ListenConfig
    {
        int ttsPort = 8000;

        int aiPort = 8001;

        // 请求 AI 的最多次数。格式不对时重试一次，所以是 2。
        int chatMaxAttempts = 2;
    };

    class NowICanListen
    {
    public:
        explicit NowICanListen(
            ListenConfig config = ListenConfig()
        );

        // 不拥有，调用方保证它活得比本对象久。
        void SetChatAPI(
            API::IChatAPI* chatApi
        );

        // 处理一句用户话语。会阻塞到两个服务器都响应为止，
        // 请在工作线程里调用。
        //
        // isPreviousCycleUnfinished：
        //   在“即将发给 ai_server”的那一刻调用。
        //   返回 true 就把 busy 改成 true。
        ListenResult Process(
            const std::string& utteranceJson,
            const std::function<bool()>& isPreviousCycleUnfinished
        );

    private:
        // 请求 AI，校验格式，不合法时重试。
        // 返回 false 表示彻底没有可用的回复。
        // actionsAllowed 为 false 表示只能播语音、不能做动作。
        bool AskChat(
            const std::string& userText,
            API::ChatReply& reply,
            bool& actionsAllowed,
            std::vector<ListenError>& errors
        );

        ListenConfig m_config;

        API::IChatAPI* m_chatApi = nullptr;
    };
}
