#pragma once

#include <string>

// ============================================================
// AIPlatform.h
//
// 职责：AI 层用到的 Windows 小工具。
//
//   1. UTF-8 与 UTF-16 互转。
//   2. 线程安全的日志（中文不乱码）。
//   3. 访问本机三个服务器的 HTTP 客户端（WinHTTP）。
//   4. 删除文件。
//
// 本头文件不包含 Windows.h，可以放心被任何地方 include。
// ============================================================

namespace Ratuber::AI::Platform
{
    // ============================================================
    // 编码
    // ============================================================

    std::wstring Utf8ToWide(const std::string& text);

    std::string WideToUtf8(const std::wstring& text);


    // ============================================================
    // 日志
    //
    // 有控制台时直接写控制台，同时写进调试器输出窗口。
    // 格式：[tag] message
    // ============================================================

    void Log(
        const std::string& tag,
        const std::string& message
    );


    // ============================================================
    // 文件
    // ============================================================

    // 删除文件。文件被占用时重试几次。
    // 文件本来就不存在也算成功。
    bool DeleteFileWithRetry(
        const std::wstring& path,
        int retries = 5
    );

    bool FileExists(const std::wstring& path);


    // ============================================================
    // HTTP（只访问 127.0.0.1）
    // ============================================================

    namespace Http
    {
        struct Response
        {
            // 是否拿到了 HTTP 响应（连不上、超时就是 false）。
            bool connected = false;

            int status = 0;

            std::string body;

            // connected 为 false 时的原因。
            std::string error;

            bool Ok() const
            {
                return connected && status >= 200 && status < 300;
            }
        };

        Response Get(
            int port,
            const std::string& path,
            int timeoutMs = 3000
        );

        Response PostJson(
            int port,
            const std::string& path,
            const std::string& jsonBody,
            int timeoutMs = 15000
        );

        // 把失败的响应整理成一句人话，用于报错。
        std::string Describe(const Response& response);
    }
}
