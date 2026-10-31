#pragma once

#include <string>
#include <vector>

#include "API/IChatAPI.h"

// ============================================================
// AIProtocol.h
//
// 职责：集中定义 AI 层用到的所有 JSON 格式，以及对应的解析 / 生成函数。
//
//   用户话语        SpeechRecognition  ->  NowICanListen
//   AI 回复         IChatAPI           ->  NowICanListen
//   /decide         NowICanListen      <->  ai_server.py
//   /tts            NowICanListen      <->  tts_server.py
//   /tts/{task_id}  NowICanTalk        <->  tts_server.py
//
// 本文件不依赖 Windows，所有函数都是纯函数。
// ============================================================

namespace Ratuber::AI::Protocol
{
    // tts_server.py 的单次文本上限（按字符数，不是字节数）。
    inline constexpr int kTtsMaxChars = 200;


    // ============================================================
    // 通用小工具
    // ============================================================

    bool IsValidEmotion(const std::string& emotion);

    bool IsValidMaxLevel(const std::string& level);

    // 去掉首尾的空白字符。
    std::string Trim(const std::string& text);

    // UTF-8 字符数（按码点算，和 Python 的 len() 一致）。
    size_t Utf8Length(const std::string& text);

    // 最多保留 maxChars 个字符，不会把一个汉字切成两半。
    std::string TruncateUtf8(const std::string& text, size_t maxChars);


    // ============================================================
    // 用户话语：{"status":"ok","text":"今天天气怎么样"}
    // ============================================================

    struct Utterance
    {
        // "ok" / "too_long"
        std::string status;

        std::string text;
    };

    bool ParseUtterance(
        const std::string& json,
        Utterance& out,
        std::string& error
    );


    // ============================================================
    // AI 回复：{"text","emotion","max_level","client_state":{"busy"}}
    // ============================================================

    enum class ReplyStatus
    {
        Ok,

        // 回复里找不到 JSON，或者 JSON 不合法。
        NotJson,

        // JSON 合法，但是 text 缺失或为空。
        EmptyText,

        // JSON 合法、text 有内容，但是 emotion 不在 10 种情绪里。
        // 此时 reply.text 仍然有值，可以降级为只播语音。
        InvalidEmotion,
    };

    ReplyStatus ParseChatReply(
        const std::string& raw,
        API::ChatReply& out,
        std::string& error
    );


    // ============================================================
    // ai_server.py  POST /decide
    // ============================================================

    // 生成请求体。整段 JSON 原样发给 ai_server.py。
    // busy 由 AIManager 决定，不使用 AI 自己填的值。
    std::string BuildDecideRequest(
        const API::ChatReply& reply,
        bool busy
    );

    struct Actions
    {
        // 为空表示没有。
        std::string motion;
        std::string expression;

        bool Empty() const
        {
            return motion.empty() && expression.empty();
        }
    };

    // 只截取响应里的 "actions" 部分。
    bool ParseDecideResponse(
        const std::string& json,
        Actions& actions,
        std::string& level,
        std::string& error
    );


    // ============================================================
    // tts_server.py  POST /tts
    // ============================================================

    std::string BuildTtsRequest(
        const std::string& text
    );

    struct TtsSegment
    {
        // 从 1 开始。
        int index = 0;

        int count = 0;

        std::string text;

        // 例如 3f9a_part_02_of_03.wav（只有文件名，没有目录）。
        std::string filename;
    };

    struct TtsSubmit
    {
        std::string taskId;

        // 已按 index 从小到大排好。
        std::vector<TtsSegment> segments;
    };

    bool ParseTtsSubmitResponse(
        const std::string& json,
        TtsSubmit& out,
        std::string& error
    );


    // ============================================================
    // tts_server.py  GET /tts/{task_id}
    // ============================================================

    enum class SegmentState
    {
        Queued,
        Processing,
        Completed,
        Failed,
    };

    struct TtsSegmentStatus
    {
        int index = 0;

        SegmentState state = SegmentState::Queued;

        std::string filename;
    };

    struct TtsStatus
    {
        std::string jobStatus;

        std::vector<TtsSegmentStatus> segments;
    };

    bool ParseTtsStatusResponse(
        const std::string& json,
        TtsStatus& out,
        std::string& error
    );
}
