#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

#include "DeepSeekChatAPI.h"

#include <vector>

#include <nlohmann/json.hpp>

#include "../AIPlatform.h"
#include "../AIProtocol.h"

namespace Ratuber::AI::API
{
    namespace
    {
        constexpr const char* kTag = "DeepSeek";

        constexpr std::size_t kErrorBodyBytes = 200;

#ifdef _WIN32
        class WinHttpHandle
        {
        public:
            explicit WinHttpHandle(HINTERNET value = nullptr) : m_handle(value) {}

            ~WinHttpHandle()
            {
                if (m_handle)
                {
                    WinHttpCloseHandle(m_handle);
                }
            }

            WinHttpHandle(const WinHttpHandle&) = delete;
            WinHttpHandle& operator=(const WinHttpHandle&) = delete;

            HINTERNET Get() const { return m_handle; }

            explicit operator bool() const { return m_handle != nullptr; }

        private:
            HINTERNET m_handle;
        };

        bool PostHttps(
            HINTERNET session,
            const DeepSeekChatConfig& config,
            const std::string& apiKey,
            const std::string& body,
            std::string& response,
            std::string& error)
        {
            const std::wstring host = Platform::Utf8ToWide(config.host);
            const std::wstring path = Platform::Utf8ToWide(config.path);

            const std::wstring headers =
                L"Content-Type: application/json\r\n"
                L"Authorization: Bearer " + Platform::Utf8ToWide(apiKey) + L"\r\n";

            WinHttpHandle connect(
                WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));

            if (!connect)
            {
                error = "WinHttpConnect 失败：" + std::to_string(GetLastError());
                return false;
            }

            WinHttpHandle request(
                WinHttpOpenRequest(
                    connect.Get(), L"POST", path.c_str(), nullptr,
                    WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                    WINHTTP_FLAG_SECURE));

            if (!request)
            {
                error = "WinHttpOpenRequest 失败：" + std::to_string(GetLastError());
                return false;
            }

            const DWORD bodySize = static_cast<DWORD>(body.size());

            if (!WinHttpSendRequest(
                    request.Get(),
                    headers.c_str(), static_cast<DWORD>(headers.size()),
                    const_cast<char*>(body.data()), bodySize, bodySize, 0))
            {
                error = "WinHttpSendRequest 失败：" + std::to_string(GetLastError())
                    + "（网络不通？需要代理？）";
                return false;
            }

            if (!WinHttpReceiveResponse(request.Get(), nullptr))
            {
                error = "WinHttpReceiveResponse 失败：" + std::to_string(GetLastError())
                    + "（超时或网络中断）";
                return false;
            }

            DWORD status = 0;
            DWORD statusSize = sizeof(status);

            WinHttpQueryHeaders(
                request.Get(),
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                WINHTTP_NO_HEADER_INDEX);

            std::string responseBody;

            for (;;)
            {
                DWORD available = 0;

                if (!WinHttpQueryDataAvailable(request.Get(), &available) || available == 0)
                {
                    break;
                }

                std::vector<char> buffer(available);
                DWORD read = 0;

                if (!WinHttpReadData(request.Get(), buffer.data(), available, &read))
                {
                    break;
                }

                responseBody.append(buffer.data(), read);
            }

            if (status < 200 || status >= 300)
            {
                error = "HTTP " + std::to_string(status) + "："
                    + responseBody.substr(0, kErrorBodyBytes);

                if (status == 401)
                {
                    error += "（API Key 不对？）";
                }
                else if (status == 402)
                {
                    error += "（余额不足？）";
                }

                return false;
            }

            response = std::move(responseBody);
            return true;
        }
#endif

        std::string ReadApiKey(const std::string& configured)
        {
            if (!configured.empty())
            {
                return configured;
            }

#ifdef _WIN32
            char buffer[512]{};

            const DWORD length = GetEnvironmentVariableA(
                "DEEPSEEK_API_KEY", buffer, static_cast<DWORD>(sizeof(buffer)));

            if (length > 0 && length < sizeof(buffer))
            {
                return std::string(buffer, length);
            }
#endif
            return std::string();
        }
    }


    DeepSeekChatAPI::DeepSeekChatAPI(
        DeepSeekChatConfig config,
        Transport transport
    )
        :
        m_config(std::move(config)),
        m_transport(std::move(transport))
    {
        if (m_transport)
        {
            return;
        }

#ifdef _WIN32
        const std::string apiKey = ReadApiKey(m_config.apiKey);

        HINTERNET session = WinHttpOpen(
            L"Ratuber-Chat/1.0",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

        if (session)
        {
            WinHttpSetTimeouts(
                session,
                m_config.resolveTimeoutMs, m_config.connectTimeoutMs,
                m_config.sendTimeoutMs, m_config.receiveTimeoutMs);
        }

        m_session = session;

        m_transport = [this, apiKey](
            const std::string& body, std::string& response, std::string& error)
        {
            if (apiKey.empty())
            {
                error = "没有 DeepSeek API Key（设置环境变量 DEEPSEEK_API_KEY，"
                        "或填 DeepSeekChatConfig::apiKey）";
                return false;
            }

            if (m_session == nullptr)
            {
                error = "WinHTTP 会话创建失败";
                return false;
            }

            return PostHttps(
                static_cast<HINTERNET>(m_session), m_config, apiKey, body, response, error);
        };
#else
        m_transport = [](const std::string&, std::string&, std::string& error)
        {
            error = "DeepSeekChatAPI 的默认传输层只支持 Windows";
            return false;
        };
#endif
    }

    DeepSeekChatAPI::~DeepSeekChatAPI()
    {
#ifdef _WIN32
        if (m_session != nullptr)
        {
            WinHttpCloseHandle(static_cast<HINTERNET>(m_session));
            m_session = nullptr;
        }
#endif
    }

    void DeepSeekChatAPI::ForgetExchange(const std::string& userMessage)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (auto it = m_history.rbegin(); it != m_history.rend(); ++it)
        {
            if (it->user == userMessage)
            {
                m_history.erase(std::next(it).base());

                Platform::Log(kTag, "已把被取消的一轮从对话历史里撤掉");

                return;
            }
        }
    }

    void DeepSeekChatAPI::ClearHistory()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_history.clear();
        m_lastReplyText.clear();
    }

    std::string DeepSeekChatAPI::LastReplyText() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        return m_lastReplyText;
    }

    // 调用前必须持有 m_mutex。
    std::string DeepSeekChatAPI::BuildRequestBody(const std::string& userMessage) const
    {
        nlohmann::json messages = nlohmann::json::array();

        std::string system = m_config.persona;

        if (!system.empty())
        {
            system += "\n";
        }

        system += kChatOutputRules;

        messages.push_back({ { "role", "system" }, { "content", system } });

        for (const Turn& turn : m_history)
        {
            messages.push_back({ { "role", "user" }, { "content", turn.user } });
            messages.push_back({ { "role", "assistant" }, { "content", turn.assistant } });
        }

        messages.push_back({ { "role", "user" }, { "content", userMessage } });

        nlohmann::json body;

        body["model"] = m_config.model;
        body["messages"] = std::move(messages);
        body["response_format"] = { { "type", "json_object" } };
        body["temperature"] = m_config.temperature;
        body["max_tokens"] = m_config.maxTokens;
        body["stream"] = false;

        // 不合法的 UTF-8 也不抛异常，直接替换掉。
        return body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    }

    bool DeepSeekChatAPI::SendMessage(
        const std::string& userMessage,
        std::string& reply,
        std::string& errorMessage
    )
    {
        reply.clear();
        errorMessage.clear();

        if (userMessage.empty())
        {
            errorMessage = "用户消息为空";
            return false;
        }

        // 整个请求期间持锁：同一时刻只会有一个请求，历史顺序不会乱。
        std::lock_guard<std::mutex> lock(m_mutex);

        const std::string requestBody = BuildRequestBody(userMessage);

        std::string responseBody;

        if (!m_transport(requestBody, responseBody, errorMessage))
        {
            return false;
        }

        const nlohmann::json json =
            nlohmann::json::parse(responseBody, nullptr, false);

        if (json.is_discarded() || !json.is_object()
            || !json.contains("choices") || !json["choices"].is_array()
            || json["choices"].empty()
            || !json["choices"][0].is_object()
            || !json["choices"][0].contains("message")
            || !json["choices"][0]["message"].is_object()
            || !json["choices"][0]["message"].contains("content")
            || !json["choices"][0]["message"]["content"].is_string())
        {
            errorMessage = "DeepSeek 的响应格式不对：" + responseBody.substr(0, kErrorBodyBytes);
            return false;
        }

        reply = json["choices"][0]["message"]["content"].get<std::string>();

        if (reply.empty())
        {
            errorMessage = "DeepSeek 返回了空内容";
            return false;
        }

        // 只记住格式合法的回复。
        ChatReply parsed;
        std::string parseError;

        if (Protocol::ParseChatReply(reply, parsed, parseError) == Protocol::ReplyStatus::Ok
            && m_config.maxHistoryTurns > 0)
        {
            m_history.push_back({ userMessage, reply });

            while (static_cast<int>(m_history.size()) > m_config.maxHistoryTurns)
            {
                m_history.pop_front();
            }

            m_lastReplyText = parsed.text;
        }
        else if (m_config.maxHistoryTurns > 0)
        {
            Platform::Log(kTag, "这条回复格式不合法，不放进对话历史");
        }

        return true;
    }
}
