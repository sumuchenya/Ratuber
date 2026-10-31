// ============================================================
// ChatUISmokeTest.cpp
//
// 第 2 份（界面）的冒烟测试：不需要 Live2D、不需要 AI 服务器。
// 打开一个 800×1000 的窗口，有输入框、发送按钮、气泡。
// 你输入一句话，程序用“假 AI”回复一段话，
// 模拟 AIManager 的事件：开始播放 -> 一个小句一个小句 -> 结束。
//
// 用法：
//   ChatUISmokeTest          普通窗口（浅蓝色背景）
//   ChatUISmokeTest pet      和 main.cpp 一样：无边框、置顶、洋红色透明
//
// 请逐项检查：
//   1. 能用中文输入法打字，候选词窗口能正常出现。
//   2. 回车 / 点“发送”都能发送，发送后输入框清空、光标还在。
//   3. 气泡里的字一个小句一个小句地、逐字出现；气泡大小从一开始就是定的。
//   4. 回复播放中，气泡右下角有“跳过”按钮，点了之后剩下的字超高速补完。
//   5. 播完后气泡停留几秒再消失。
//   6. pet 模式下：点输入框、按钮不会拖动窗口；点别处可以拖动窗口；
//      气泡和输入框周围没有粉红色的边。
//   7. 输入框在窗口画面刷新时不闪烁。
//   8. 输入 “错误” 两个字会让提示条显示一条红色错误；输入 “提示” 显示普通提示；
//      输入 “清除” 清掉提示。
// ============================================================

#include <SFML/Graphics.hpp>

#include <chrono>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

#include "../ChatUI.h"
#include "../BubbleModel.h"

namespace
{
    // 假的“播放”：每个小句播放 字数 × 0.22 秒。
    struct FakePlayback
    {
        bool active = false;

        std::string full;

        std::vector<std::string> segments;

        size_t next = 0;

        double timer = 0.0;

    };

    std::vector<std::string> SplitSegments(const std::string& text)
    {
        // 按 UTF-8 字符每 7 个字切一段。
        const auto u32 = Ratuber::UI::Utf8ToUtf32(text);

        std::vector<std::string> result;

        for (size_t i = 0; i < u32.size(); i += 7)
        {
            result.push_back(Ratuber::UI::Utf32ToUtf8(u32.substr(i, 7)));
        }

        return result;
    }
}

int main(int argc, char** argv)
{
    const bool pet = argc > 1 && std::string(argv[1]) == "pet";

    constexpr unsigned int width = 800;
    constexpr unsigned int height = 1000;

    sf::ContextSettings settings;

    settings.antiAliasingLevel = 0;

    sf::RenderWindow window(
        sf::VideoMode({ width, height }),
        "ChatUISmokeTest",
        pet ? sf::Style::None : sf::Style::Default,
        sf::State::Windowed,
        settings
    );

    window.setVerticalSyncEnabled(true);

#ifdef _WIN32
    HWND hwnd = window.getNativeHandle();

    if (pet)
    {
        SetWindowLongPtr(hwnd, GWL_EXSTYLE,
            GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TOPMOST);

        SetLayeredWindowAttributes(hwnd, RGB(255, 0, 255), 0, LWA_COLORKEY);
    }
#endif

    Ratuber::UI::ChatUI ui;

    if (!ui.Initialize(window))
    {
        std::printf("[UI] 初始化不完整：找不到中文字体，或输入框创建失败。\n");
    }

    FakePlayback playback;

    bool dragging = false;

#ifdef _WIN32
    POINT dragStartMouse{};
    POINT dragStartWindow{};
#endif

    ui.SetOnSubmit([&](const std::string& text)
    {
        if (text == "错误")
        {
            ui.SetStatus("AI 服务器：连接失败（这是一条测试用的错误提示）", true);

            return;
        }

        if (text == "提示")
        {
            ui.SetStatus("正在启动 AI 服务器……", false);

            return;
        }

        if (text == "清除")
        {
            ui.SetStatus("");

            return;
        }

        playback = FakePlayback();

        playback.active = true;

        playback.full = "收到啦，你说的是：" + text + "。这是一句用来测试气泡逐字显现的回复，请观察字出现的节奏。";

        playback.segments = SplitSegments(playback.full);

        ui.OnSpeechStarted(playback.full);

        // 假装 TTS 生成花了半秒。
        playback.timer = 0.5;
    });

    ui.SetOnSkip([&]()
    {
        if (playback.active)
        {
            playback.active = false;

            ui.OnSpeechFinished(playback.full, true);
        }
    });

    ui.FocusInput();

    auto last = std::chrono::steady_clock::now();

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (ui.HandleEvent(*event))
            {
                continue;
            }

            if (const auto* pressed = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (pet
                    && pressed->button == sf::Mouse::Button::Left
                    && !ui.IsPointOnUI(pressed->position))
                {
                    dragging = true;

#ifdef _WIN32
                    GetCursorPos(&dragStartMouse);

                    RECT rect{};

                    GetWindowRect(hwnd, &rect);

                    dragStartWindow = { rect.left, rect.top };
#endif
                }
            }

            if (const auto* released = event->getIf<sf::Event::MouseButtonReleased>())
            {
                if (released->button == sf::Mouse::Button::Left)
                {
                    dragging = false;
                }
            }
        }

#ifdef _WIN32
        if (dragging)
        {
            POINT now{};

            GetCursorPos(&now);

            SetWindowPos(hwnd, HWND_TOPMOST,
                dragStartWindow.x + (now.x - dragStartMouse.x),
                dragStartWindow.y + (now.y - dragStartMouse.y),
                0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        }
#endif

        const auto current = std::chrono::steady_clock::now();

        const float dt = std::chrono::duration<float>(current - last).count();

        last = current;

        // ---- 假播放的时间线 ----
        if (playback.active)
        {
            playback.timer -= dt;

            if (playback.timer <= 0.0)
            {
                if (playback.next < playback.segments.size())
                {
                    const std::string& segment = playback.segments[playback.next++];

                    ui.OnSegmentStarted(segment);

                    playback.timer = static_cast<double>(
                        Ratuber::UI::Utf8ToUtf32(segment).size()) * 0.22;
                }
                else
                {
                    playback.active = false;

                    ui.OnSpeechFinished(playback.full, false);
                }
            }
        }

        ui.Update(dt);

        if (pet)
        {
            window.clear(sf::Color(255, 0, 255));
        }
        else
        {
            window.clear(sf::Color(190, 215, 240));
        }

        ui.Draw(window);

        window.display();
    }

    ui.Shutdown();

    return 0;
}
