#include "AIProtocol.h"

#include <algorithm>
#include <cctype>
#include <iterator>

#include <nlohmann/json.hpp>

namespace Ratuber::AI::Protocol
{
    using json = nlohmann::json;

    namespace
    {
        // 取字符串字段。字段不存在或者不是字符串时返回 false。
        bool GetString(
            const json& object,
            const char* key,
            std::string& out
        )
        {
            if (!object.is_object())
            {
                return false;
            }

            const auto it = object.find(key);

            if (it == object.end() || !it->is_string())
            {
                return false;
            }

            out = it->get<std::string>();

            return true;
        }

        // 取整数字段。
        bool GetInt(
            const json& object,
            const char* key,
            int& out
        )
        {
            if (!object.is_object())
            {
                return false;
            }

            const auto it = object.find(key);

            if (it == object.end() || !it->is_number_integer())
            {
                return false;
            }

            out = it->get<int>();

            return true;
        }

        // 序列化。遇到非法 UTF-8 时替换成 U+FFFD，而不是抛异常。
        std::string Dump(const json& value)
        {
            return value.dump(
                -1,
                ' ',
                false,
                json::error_handler_t::replace
            );
        }

        std::string ToLower(std::string text)
        {
            std::transform(
                text.begin(),
                text.end(),
                text.begin(),
                [](unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                }
            );

            return text;
        }

        // 只保留文件名，丢掉任何目录部分。
        std::string BaseName(const std::string& path)
        {
            const auto pos = path.find_last_of("/\\");

            return pos == std::string::npos
                ? path
                : path.substr(pos + 1);
        }

        // 取出第一个 { 到最后一个 } 之间的内容。
        // 模型偶尔会在 JSON 外面包一层 ```json 代码块或者多说一句话。
        std::string ExtractJsonObject(const std::string& raw)
        {
            const auto first = raw.find('{');
            const auto last = raw.rfind('}');

            if (first == std::string::npos
                || last == std::string::npos
                || last < first)
            {
                return {};
            }

            return raw.substr(first, last - first + 1);
        }

        SegmentState ParseState(const std::string& state)
        {
            if (state == "completed")
            {
                return SegmentState::Completed;
            }

            if (state == "failed")
            {
                return SegmentState::Failed;
            }

            if (state == "processing")
            {
                return SegmentState::Processing;
            }

            return SegmentState::Queued;
        }
    }


    // ============================================================
    // 通用小工具
    // ============================================================

    bool IsValidEmotion(const std::string& emotion)
    {
        for (const char* valid : API::kEmotions)
        {
            if (emotion == valid)
            {
                return true;
            }
        }

        return false;
    }

    bool IsValidMaxLevel(const std::string& level)
    {
        return level == "compose"
            || level == "preset"
            || level == "atom";
    }

    std::string Trim(const std::string& text)
    {
        size_t begin = 0;
        size_t end = text.size();

        while (begin < end
            && std::isspace(static_cast<unsigned char>(text[begin])))
        {
            ++begin;
        }

        while (end > begin
            && std::isspace(static_cast<unsigned char>(text[end - 1])))
        {
            --end;
        }

        return text.substr(begin, end - begin);
    }

    size_t Utf8Length(const std::string& text)
    {
        size_t count = 0;

        for (const unsigned char c : text)
        {
            // 续字节形如 10xxxxxx，不算新字符。
            if ((c & 0xC0) != 0x80)
            {
                ++count;
            }
        }

        return count;
    }

    std::string TruncateUtf8(
        const std::string& text,
        size_t maxChars
    )
    {
        size_t count = 0;

        for (size_t i = 0; i < text.size(); ++i)
        {
            const unsigned char c =
                static_cast<unsigned char>(text[i]);

            if ((c & 0xC0) == 0x80)
            {
                continue;
            }

            if (count == maxChars)
            {
                return text.substr(0, i);
            }

            ++count;
        }

        return text;
    }


    // ============================================================
    // 用户话语
    // ============================================================

    bool ParseUtterance(
        const std::string& text,
        Utterance& out,
        std::string& error
    )
    {
        out = {};

        const json root = json::parse(text, nullptr, false);

        if (root.is_discarded() || !root.is_object())
        {
            error = "用户话语不是合法的 JSON 对象";

            return false;
        }

        if (!GetString(root, "status", out.status))
        {
            error = "用户话语缺少 status 字段";

            return false;
        }

        // text 缺失按空处理。
        GetString(root, "text", out.text);

        return true;
    }


    // ============================================================
    // AI 回复
    // ============================================================

    ReplyStatus ParseChatReply(
        const std::string& raw,
        API::ChatReply& out,
        std::string& error
    )
    {
        out = {};

        const std::string body = ExtractJsonObject(raw);

        if (body.empty())
        {
            error = "AI 回复里找不到 JSON";

            return ReplyStatus::NotJson;
        }

        const json root = json::parse(body, nullptr, false);

        if (root.is_discarded() || !root.is_object())
        {
            error = "AI 回复不是合法的 JSON";

            return ReplyStatus::NotJson;
        }

        std::string text;

        GetString(root, "text", text);

        text = Trim(text);

        if (text.empty())
        {
            error = "AI 回复的 text 为空";

            return ReplyStatus::EmptyText;
        }

        out.text = text;

        std::string maxLevel;

        GetString(root, "max_level", maxLevel);

        maxLevel = ToLower(Trim(maxLevel));

        out.maxLevel = IsValidMaxLevel(maxLevel)
            ? maxLevel
            : "compose";

        std::string emotion;

        GetString(root, "emotion", emotion);

        emotion = ToLower(Trim(emotion));

        if (!IsValidEmotion(emotion))
        {
            error = "AI 回复的 emotion 不合法: \"" + emotion + "\"";

            return ReplyStatus::InvalidEmotion;
        }

        out.emotion = emotion;

        return ReplyStatus::Ok;
    }


    // ============================================================
    // ai_server.py
    // ============================================================

    std::string BuildDecideRequest(
        const API::ChatReply& reply,
        bool busy
    )
    {
        json request;

        request["text"] = reply.text;
        request["emotion"] = reply.emotion;
        request["max_level"] = reply.maxLevel;
        request["client_state"] = { { "busy", busy } };

        return Dump(request);
    }

    bool ParseDecideResponse(
        const std::string& text,
        Actions& actions,
        std::string& level,
        std::string& error
    )
    {
        actions = {};
        level.clear();

        const json root = json::parse(text, nullptr, false);

        if (root.is_discarded() || !root.is_object())
        {
            error = "ai_server 的响应不是合法的 JSON";

            return false;
        }

        GetString(root, "level", level);

        const auto it = root.find("actions");

        if (it == root.end() || !it->is_object())
        {
            error = "ai_server 的响应里没有 actions";

            return false;
        }

        // motion / expression 为 null 时 GetString 返回 false，保持为空。
        GetString(*it, "motion", actions.motion);
        GetString(*it, "expression", actions.expression);

        return true;
    }


    // ============================================================
    // tts_server.py
    // ============================================================

    std::string BuildTtsRequest(const std::string& text)
    {
        json request;

        request["text"] = text;

        return Dump(request);
    }

    bool ParseTtsSubmitResponse(
        const std::string& text,
        TtsSubmit& out,
        std::string& error
    )
    {
        out = {};

        const json root = json::parse(text, nullptr, false);

        if (root.is_discarded() || !root.is_object())
        {
            error = "tts_server 的响应不是合法的 JSON";

            return false;
        }

        if (!GetString(root, "task_id", out.taskId)
            || out.taskId.empty())
        {
            error = "tts_server 的响应缺少 task_id";

            return false;
        }

        const auto it = root.find("segments");

        if (it == root.end() || !it->is_array() || it->empty())
        {
            error = "tts_server 的响应缺少 segments"
                "（tts_server.py 是不是还是没有小句拆分的旧版？）";

            return false;
        }

        for (const json& item : *it)
        {
            TtsSegment segment;

            if (!GetInt(item, "segment_index", segment.index)
                || segment.index < 1)
            {
                error = "tts_server 的 segments 里有缺少 segment_index 的项";

                return false;
            }

            GetInt(item, "segment_count", segment.count);
            GetString(item, "text", segment.text);

            std::string filename;

            GetString(item, "filename", filename);

            segment.filename = BaseName(filename);

            if (segment.filename.empty())
            {
                error = "tts_server 的 segments 里有缺少 filename 的项";

                return false;
            }

            out.segments.push_back(std::move(segment));
        }

        std::sort(
            out.segments.begin(),
            out.segments.end(),
            [](const TtsSegment& a, const TtsSegment& b)
            {
                return a.index < b.index;
            }
        );

        return true;
    }

    bool ParseTtsStatusResponse(
        const std::string& text,
        TtsStatus& out,
        std::string& error
    )
    {
        out = {};

        const json root = json::parse(text, nullptr, false);

        if (root.is_discarded() || !root.is_object())
        {
            error = "tts_server 的状态响应不是合法的 JSON";

            return false;
        }

        GetString(root, "status", out.jobStatus);

        const auto it = root.find("segments");

        if (it == root.end() || !it->is_array())
        {
            error = "tts_server 的状态响应缺少 segments";

            return false;
        }

        for (const json& item : *it)
        {
            TtsSegmentStatus segment;

            if (!GetInt(item, "segment_index", segment.index))
            {
                continue;
            }

            std::string state;

            GetString(item, "status", state);

            segment.state = ParseState(state);

            std::string filename;

            GetString(item, "filename", filename);

            segment.filename = BaseName(filename);

            out.segments.push_back(std::move(segment));
        }

        return true;
    }
}
