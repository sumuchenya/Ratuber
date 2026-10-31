#include "NowICanListen.h"

#include <future>

#include "../AIPlatform.h"
#include "../AIProtocol.h"

namespace Ratuber::AI
{
    namespace
    {
        constexpr const char* kTag = "NowICanListen";

        // 用户整轮说太久被强制结束时的固定回复。
        // 不调 AI，直接写死：害羞，并且 busy 强制为 true。
        constexpr const char* kTooLongText = "再说一遍嘛";
        constexpr const char* kTooLongEmotion = "shy";
    }

    NowICanListen::NowICanListen(ListenConfig config)
        :
        m_config(config)
    {}

    void NowICanListen::SetChatAPI(API::IChatAPI* chatApi)
    {
        m_chatApi = chatApi;
    }


    // ============================================================
    // 请求 AI
    // ============================================================

    bool NowICanListen::AskChat(
        const std::string& userText,
        API::ChatReply& reply,
        bool& actionsAllowed,
        std::vector<ListenError>& errors
    )
    {
        if (m_chatApi == nullptr)
        {
            errors.push_back({ "LLM", "IChatAPI 还没有设置（SetChatAPI）" });

            return false;
        }

        std::string lastRaw;
        std::string lastError;

        // emotion 不合法但 text 可用的那次回复，用来降级。
        API::ChatReply fallback;
        bool hasFallback = false;

        for (int attempt = 1; attempt <= m_config.chatMaxAttempts; ++attempt)
        {
            std::string raw;
            std::string apiError;

            if (!m_chatApi->SendMessage(userText, raw, apiError))
            {
                lastError = apiError.empty() ? "请求 AI 失败" : apiError;

                Platform::Log(kTag, "第 " + std::to_string(attempt)
                    + " 次请求 AI 失败：" + lastError);

                continue;
            }

            lastRaw = raw;

            API::ChatReply parsed;
            std::string parseError;

            const Protocol::ReplyStatus status =
                Protocol::ParseChatReply(raw, parsed, parseError);

            if (status == Protocol::ReplyStatus::Ok)
            {
                reply = parsed;
                actionsAllowed = true;

                return true;
            }

            lastError = parseError;

            Platform::Log(kTag, "第 " + std::to_string(attempt)
                + " 次 AI 回复格式不对：" + parseError);

            if (status == Protocol::ReplyStatus::InvalidEmotion)
            {
                fallback = parsed;
                hasFallback = true;
            }
        }

        // ---- 重试之后仍然不合格：降级 ----

        // emotion 不合法：语音照播，不做动作。
        if (hasFallback)
        {
            reply = fallback;
            reply.emotion.clear();
            actionsAllowed = false;

            errors.push_back({
                "LLM",
                lastError + "，已降级为只播语音、不做动作"
            });

            return true;
        }

        // 模型完全没有按 JSON 回复，只是说了一句话：当作纯文本播出去。
        const std::string plain = Protocol::Trim(lastRaw);

        if (!plain.empty() && plain.find('{') == std::string::npos)
        {
            reply = {};
            reply.text = plain;
            actionsAllowed = false;

            errors.push_back({
                "LLM",
                "AI 没有按 JSON 格式回复，已把原文当作语音文本、不做动作"
            });

            return true;
        }

        errors.push_back({
            "LLM",
            lastError.empty() ? "AI 没有返回可用的回复" : lastError
        });

        return false;
    }


    // ============================================================
    // 处理一句话
    // ============================================================

    ListenResult NowICanListen::Process(
        const std::string& utteranceJson,
        const std::function<bool()>& isPreviousCycleUnfinished
    )
    {
        ListenResult result;

        // ---- 1. 解析用户话语 ----

        Protocol::Utterance utterance;
        std::string error;

        if (!Protocol::ParseUtterance(utteranceJson, utterance, error))
        {
            result.errors.push_back({ "输入", error });

            return result;
        }

        // ---- 2. 得到 AI 的回复 ----

        API::ChatReply reply;

        // 能不能做动作（emotion 合法才行）。
        bool actionsAllowed = true;

        // too_long 时强制 busy。
        bool forceBusy = false;

        if (utterance.status == "too_long")
        {
            reply.text = kTooLongText;
            reply.emotion = kTooLongEmotion;
            reply.maxLevel = "compose";

            forceBusy = true;

            Platform::Log(kTag, "用户这一轮说得太久，回复固定的“再说一遍嘛”");
        }
        else if (utterance.status == "ok")
        {
            const std::string userText = Protocol::Trim(utterance.text);

            if (userText.empty())
            {
                result.skippedReason = "用户文本为空";

                return result;
            }

            Platform::Log(kTag, "用户说：" + userText);

            if (!AskChat(userText, reply, actionsAllowed, result.errors))
            {
                return result;
            }
        }
        else
        {
            result.errors.push_back({
                "输入",
                "未知的 status：" + utterance.status
            });

            return result;
        }

        // ---- 3. 文本不能超过 tts_server 的上限 ----

        if (Protocol::Utf8Length(reply.text) > Protocol::kTtsMaxChars)
        {
            Platform::Log(kTag, "AI 回复超过 "
                + std::to_string(Protocol::kTtsMaxChars)
                + " 字，已截断");

            reply.text = Protocol::TruncateUtf8(
                reply.text,
                Protocol::kTtsMaxChars
            );
        }

        Platform::Log(kTag, "AI 回复：" + reply.text
            + (reply.emotion.empty()
                ? "（没有合法情绪）"
                : "  [" + reply.emotion + "]"));

        result.replyText = reply.text;
        result.emotion = reply.emotion;

        // ---- 4. 同时发给两个服务器 ----

        // 文本单独发给 tts_server.py。
        auto ttsFuture = std::async(
            std::launch::async,
            [this, &reply]()
            {
                return Platform::Http::PostJson(
                    m_config.ttsPort,
                    "/tts",
                    Protocol::BuildTtsRequest(reply.text)
                );
            }
        );

        // 整段 JSON 发给 ai_server.py。
        Platform::Http::Response decideResponse;

        bool decideSent = false;

        if (actionsAllowed && !reply.emotion.empty())
        {
            // 上一个周期没结束，就把 busy 改成 true。
            const bool busy = forceBusy
                || (isPreviousCycleUnfinished
                    && isPreviousCycleUnfinished());

            result.decideRequestJson =
                Protocol::BuildDecideRequest(reply, busy);

            Platform::Log(kTag, std::string("busy = ")
                + (busy ? "true" : "false"));

            decideResponse = Platform::Http::PostJson(
                m_config.aiPort,
                "/decide",
                result.decideRequestJson
            );

            decideSent = true;
        }

        const Platform::Http::Response ttsResponse = ttsFuture.get();

        // ---- 5. tts_server 的输出 -> NowICanTalk ----

        if (!ttsResponse.Ok())
        {
            result.errors.push_back({
                "TTS 服务器",
                Platform::Http::Describe(ttsResponse)
            });
        }
        else
        {
            Protocol::TtsSubmit submit;

            if (Protocol::ParseTtsSubmitResponse(
                    ttsResponse.body, submit, error))
            {
                result.ttsResponseJson = ttsResponse.body;
                result.hasSpeech = true;
            }
            else
            {
                result.errors.push_back({ "TTS 服务器", error });
            }
        }

        // ---- 6. ai_server 的输出 -> NowICanPlay ----

        if (decideSent)
        {
            if (!decideResponse.Ok())
            {
                result.errors.push_back({
                    "AI 服务器",
                    Platform::Http::Describe(decideResponse)
                });
            }
            else
            {
                Protocol::Actions actions;
                std::string level;

                if (Protocol::ParseDecideResponse(
                        decideResponse.body, actions, level, error))
                {
                    result.decideResponseJson = decideResponse.body;

                    // level 为 idle 时 actions 是空的，没有东西可表达。
                    result.hasActions = !actions.Empty();

                    Platform::Log(kTag, "ai_server 决定：level="
                        + level
                        + " motion="
                        + (actions.motion.empty() ? "无" : actions.motion)
                        + " expression="
                        + (actions.expression.empty() ? "无" : actions.expression));
                }
                else
                {
                    result.errors.push_back({ "AI 服务器", error });
                }
            }
        }

        return result;
    }
}
