#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <winhttp.h>

#include "AIPlatform.h"

#include <mutex>

#pragma comment(lib, "winhttp.lib")

namespace Ratuber::AI::Platform
{
    // ============================================================
    // 编码
    // ============================================================

    std::wstring Utf8ToWide(const std::string& text)
    {
        if (text.empty())
        {
            return {};
        }

        const int length = MultiByteToWideChar(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0
        );

        if (length <= 0)
        {
            return {};
        }

        std::wstring wide(static_cast<size_t>(length), L'\0');

        MultiByteToWideChar(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            wide.data(),
            length
        );

        return wide;
    }

    std::string WideToUtf8(const std::wstring& text)
    {
        if (text.empty())
        {
            return {};
        }

        const int length = WideCharToMultiByte(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0,
            nullptr,
            nullptr
        );

        if (length <= 0)
        {
            return {};
        }

        std::string utf8(static_cast<size_t>(length), '\0');

        WideCharToMultiByte(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            utf8.data(),
            length,
            nullptr,
            nullptr
        );

        return utf8;
    }


    // ============================================================
    // 日志
    // ============================================================

    void Log(
        const std::string& tag,
        const std::string& message
    )
    {
        static std::mutex logMutex;

        std::lock_guard<std::mutex> lock(logMutex);

        const std::wstring line =
            L"[" + Utf8ToWide(tag) + L"] "
            + Utf8ToWide(message) + L"\n";

        const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

        DWORD mode = 0;

        if (out != nullptr
            && out != INVALID_HANDLE_VALUE
            && GetConsoleMode(out, &mode))
        {
            DWORD written = 0;

            WriteConsoleW(
                out,
                line.c_str(),
                static_cast<DWORD>(line.size()),
                &written,
                nullptr
            );
        }

        OutputDebugStringW(line.c_str());
    }


    // ============================================================
    // 文件
    // ============================================================

    bool FileExists(const std::wstring& path)
    {
        return GetFileAttributesW(path.c_str())
            != INVALID_FILE_ATTRIBUTES;
    }

    bool DeleteFileWithRetry(
        const std::wstring& path,
        int retries
    )
    {
        for (int attempt = 0; attempt < retries; ++attempt)
        {
            if (DeleteFileW(path.c_str()))
            {
                return true;
            }

            const DWORD error = GetLastError();

            if (error == ERROR_FILE_NOT_FOUND
                || error == ERROR_PATH_NOT_FOUND)
            {
                return true;
            }

            Sleep(50);
        }

        return false;
    }


    // ============================================================
    // HTTP
    // ============================================================

    namespace Http
    {
        namespace
        {
            // 离开作用域时自动关闭句柄。
            struct Handle
            {
                HINTERNET value = nullptr;

                Handle() = default;

                Handle(const Handle&) = delete;
                Handle& operator=(const Handle&) = delete;

                ~Handle()
                {
                    if (value != nullptr)
                    {
                        WinHttpCloseHandle(value);
                    }
                }
            };

            std::string LastError(const char* where)
            {
                return std::string(where)
                    + " 失败（WinHTTP 错误码 "
                    + std::to_string(GetLastError()) + "）";
            }

            Response Perform(
                const wchar_t* verb,
                int port,
                const std::string& path,
                const std::string* body,
                int timeoutMs
            )
            {
                Response response;

                // 访问的是本机服务器，不能走系统代理。
                Handle session;

                session.value = WinHttpOpen(
                    L"Ratuber-AI/1.0",
                    WINHTTP_ACCESS_TYPE_NO_PROXY,
                    WINHTTP_NO_PROXY_NAME,
                    WINHTTP_NO_PROXY_BYPASS,
                    0
                );

                if (session.value == nullptr)
                {
                    response.error = LastError("WinHttpOpen");

                    return response;
                }

                WinHttpSetTimeouts(
                    session.value,
                    2000,
                    2000,
                    timeoutMs,
                    timeoutMs
                );

                Handle connection;

                connection.value = WinHttpConnect(
                    session.value,
                    L"127.0.0.1",
                    static_cast<INTERNET_PORT>(port),
                    0
                );

                if (connection.value == nullptr)
                {
                    response.error = LastError("WinHttpConnect");

                    return response;
                }

                const std::wstring widePath = Utf8ToWide(path);

                Handle request;

                request.value = WinHttpOpenRequest(
                    connection.value,
                    verb,
                    widePath.c_str(),
                    nullptr,
                    WINHTTP_NO_REFERER,
                    WINHTTP_DEFAULT_ACCEPT_TYPES,
                    0
                );

                if (request.value == nullptr)
                {
                    response.error = LastError("WinHttpOpenRequest");

                    return response;
                }

                const wchar_t* headers =
                    L"Content-Type: application/json; charset=utf-8\r\n";

                BOOL sent = FALSE;

                if (body != nullptr)
                {
                    const DWORD size = static_cast<DWORD>(body->size());

                    sent = WinHttpSendRequest(
                        request.value,
                        headers,
                        static_cast<DWORD>(-1),
                        const_cast<char*>(body->data()),
                        size,
                        size,
                        0
                    );
                }
                else
                {
                    sent = WinHttpSendRequest(
                        request.value,
                        WINHTTP_NO_ADDITIONAL_HEADERS,
                        0,
                        WINHTTP_NO_REQUEST_DATA,
                        0,
                        0,
                        0
                    );
                }

                if (!sent)
                {
                    response.error =
                        "无法连接 127.0.0.1:" + std::to_string(port)
                        + "（" + LastError("WinHttpSendRequest") + "）";

                    return response;
                }

                if (!WinHttpReceiveResponse(request.value, nullptr))
                {
                    response.error =
                        "127.0.0.1:" + std::to_string(port)
                        + " 没有响应（" + LastError("WinHttpReceiveResponse") + "）";

                    return response;
                }

                DWORD status = 0;
                DWORD statusSize = sizeof(status);

                WinHttpQueryHeaders(
                    request.value,
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX,
                    &status,
                    &statusSize,
                    WINHTTP_NO_HEADER_INDEX
                );

                response.connected = true;
                response.status = static_cast<int>(status);

                for (;;)
                {
                    DWORD available = 0;

                    if (!WinHttpQueryDataAvailable(request.value, &available)
                        || available == 0)
                    {
                        break;
                    }

                    std::string chunk(available, '\0');

                    DWORD read = 0;

                    if (!WinHttpReadData(
                            request.value,
                            chunk.data(),
                            available,
                            &read)
                        || read == 0)
                    {
                        break;
                    }

                    response.body.append(chunk.data(), read);
                }

                return response;
            }
        }

        Response Get(
            int port,
            const std::string& path,
            int timeoutMs
        )
        {
            return Perform(L"GET", port, path, nullptr, timeoutMs);
        }

        Response PostJson(
            int port,
            const std::string& path,
            const std::string& jsonBody,
            int timeoutMs
        )
        {
            return Perform(L"POST", port, path, &jsonBody, timeoutMs);
        }

        std::string Describe(const Response& response)
        {
            if (!response.connected)
            {
                return response.error.empty()
                    ? "无法连接服务器"
                    : response.error;
            }

            std::string body = response.body;

            if (body.size() > 300)
            {
                body.resize(300);
                body += "...";
            }

            return "HTTP " + std::to_string(response.status)
                + "：" + body;
        }
    }
}
