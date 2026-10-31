#pragma once

#include <deque>
#include <functional>
#include <mutex>
#include <string>

#include "IChatAPI.h"

// ============================================================
// DeepSeekChatAPI.h
//
// IChatAPI 的 DeepSeek 实现。
//
//   - 系统提示词 = 人设（persona） + kChatOutputRules
//   - 打开 JSON 输出模式（response_format = json_object）
//   - 保留最近 maxHistoryTurns 轮对话作为上下文
//   - 只有“格式合法”的回复才会进入历史，
//     所以 NowICanListen 因为格式不对而重试时，坏回复不会污染上下文
//
// API Key：
//   优先用 Config::apiKey；为空则读环境变量 DEEPSEEK_API_KEY。
//   （和 voicerecognitionoptimisation.cpp 用的是同一个 key。）
//
// 线程：SendMessage 可在任意线程调用，内部串行。
// ============================================================

namespace Ratuber::AI::API
{
    struct DeepSeekChatConfig
    {
        // 为空时读环境变量 DEEPSEEK_API_KEY。
        std::string apiKey;

        std::string host = "api.deepseek.com";
        std::string path = "/chat/completions";
        std::string model = "deepseek-chat";

        // 回复只有 50 字左右的 JSON，不需要太多。
        int maxTokens = 300;

        double temperature = 1.0;

        // 保留最近多少轮（一问一答算一轮）。0 表示不带历史。
        int maxHistoryTurns = 6;

        int resolveTimeoutMs = 5000;
        int connectTimeoutMs = 5000;
        int sendTimeoutMs = 10000;
        int receiveTimeoutMs = 30000;

        // 人设。写在 kChatOutputRules 前面。想改角色性格就改这里。
        std::string persona =
            "你是 Ratuber，一位可爱、活泼、有点小魔法的虚拟主播，"
            "正在和用户语音聊天。你说话简短自然，像朋友一样，"
            "带一点俏皮，但不啰嗦。用户的话来自语音识别，"
            "偶尔有错别字，请结合语境理解。";
    };

    class DeepSeekChatAPI : public IChatAPI
    {
    public:
        // 传输层：发出请求体，拿回响应体。
        // 默认用 WinHTTP 访问 DeepSeek；测试时可以换成假的。
        using Transport = std::function<bool(
            const std::string& requestBody,
            std::string& responseBody,
            std::string& errorMessage)>;

        explicit DeepSeekChatAPI(
            DeepSeekChatConfig config = DeepSeekChatConfig(),
            Transport transport = nullptr
        );

        ~DeepSeekChatAPI() override;

        DeepSeekChatAPI(const DeepSeekChatAPI&) = delete;
        DeepSeekChatAPI& operator=(const DeepSeekChatAPI&) = delete;

        bool SendMessage(
            const std::string& userMessage,
            std::string& reply,
            std::string& errorMessage
        ) override;

        // 把最近一轮里用户说的是 userMessage 的那一轮撤掉。
        void ForgetExchange(const std::string& userMessage) override;

        // 清空对话历史。
        void ClearHistory();

        // 最近一次成功回复的 text，用来喂给语音纠错
        // （TranscriptOptimiser::SetLastAssistantReply）。
        std::string LastReplyText() const;

    private:
        struct Turn
        {
            std::string user;
            std::string assistant;
        };

        std::string BuildRequestBody(const std::string& userMessage) const;

        DeepSeekChatConfig m_config;
        Transport m_transport;

        mutable std::mutex m_mutex;
        std::deque<Turn> m_history;
        std::string m_lastReplyText;

        void* m_session = nullptr;
    };
}
