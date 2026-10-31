#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include <GL/glew.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "Live2DModel/Live2DModel.h"

#include "motion/MotionManager.h"
#include "expression/ExpressionManager.h"

#include "blink/BlinkManager.h"
#include "MouthOpen/MouthOpenManager.h"

#include "AI/AIManager.h"
#include "AI/API/DeepSeekChatAPI.h"

#include "UI/ChatUI.h"


namespace
{
    // AIManager 的回调在后台线程里触发，只把消息存起来，主线程每帧取走。
    struct Mailbox
    {
        std::mutex mutex;

        std::vector<std::string> errors;

        bool ready = false;

        void PushError(const std::string& source, const std::string& message)
        {
            std::lock_guard<std::mutex> lock(mutex);

            errors.push_back(source + "：" + message);
        }

        void SetReady()
        {
            std::lock_guard<std::mutex> lock(mutex);

            ready = true;
        }
    };

    Ratuber::UI::ItemStatus ToItemStatus(Ratuber::AI::TaskState state)
    {
        using Ratuber::AI::TaskState;
        using Ratuber::UI::ItemStatus;

        switch (state)
        {
        case TaskState::Waiting:   return ItemStatus::Waiting;
        case TaskState::Thinking:  return ItemStatus::Thinking;
        case TaskState::Ready:     return ItemStatus::Ready;
        case TaskState::Speaking:  return ItemStatus::Speaking;
        case TaskState::Done:      return ItemStatus::Done;
        case TaskState::Skipped:   return ItemStatus::Skipped;
        case TaskState::Cancelled: return ItemStatus::Cancelled;
        case TaskState::Failed:    return ItemStatus::Failed;
        }

        return ItemStatus::Done;
    }
}


int main()
{
    namespace fs = std::filesystem;


    // 资源目录用 CMake 里定义好的宏，不依赖从哪个目录启动。
    const fs::path resourceRoot = RATUBER_MAO_ASSET_DIR;

    constexpr unsigned int windowWidth = 800;
    constexpr unsigned int windowHeight = 1000;


    // 模型是围绕窗口中心缩放的（Live2DModel 里 SetCenterPosition(0, 0)）。
    constexpr float kModelCenterX = 400.0f;
    constexpr float kModelCenterY = 500.0f;

    // 缩放为 1 时，帽子顶在窗口里的位置（用 ruler.ps1 量的）。
    constexpr float kHeadTopAtScale1 = 545.0f;
    constexpr float kHeadCenterXAtScale1 = 240.0f;

    auto headTopFor = [&](float scale)
        {
            return kModelCenterY + (kHeadTopAtScale1 - kModelCenterY) * scale;
        };

    auto headCenterXFor = [&](float scale)
        {
            return kModelCenterX + (kHeadCenterXAtScale1 - kModelCenterX) * scale;
        };


    sf::ContextSettings settings;

    settings.depthBits = 24;
    settings.stencilBits = 8;
    // 不开抗锯齿：窗口用洋红色当透明色，抗锯齿会在边缘混出粉边。
    settings.antiAliasingLevel = 0;
    settings.majorVersion = 3;
    settings.minorVersion = 3;


    sf::RenderWindow window(
        sf::VideoMode({ windowWidth, windowHeight }),
        "Ratuber",
        sf::Style::None,
        sf::State::Windowed,
        settings
    );


    if (!window.isOpen())
    {
        return 1;
    }


    HWND hwnd = window.getNativeHandle();


    LONG_PTR exStyle =
        GetWindowLongPtr(
            hwnd,
            GWL_EXSTYLE
        );


    SetWindowLongPtr(
        hwnd,
        GWL_EXSTYLE,
        exStyle |
        WS_EX_LAYERED |
        WS_EX_TOPMOST
    );


    if (!SetLayeredWindowAttributes(
        hwnd,
        RGB(255, 0, 255),
        0,
        LWA_COLORKEY))
    {
        return 1;
    }


    const int screenWidth =
        GetSystemMetrics(SM_CXSCREEN);

    const int screenHeight =
        GetSystemMetrics(SM_CYSCREEN);


    SetWindowPos(
        hwnd,
        HWND_TOPMOST,
        (screenWidth - windowWidth) / 2,
        (screenHeight - windowHeight) / 2,
        windowWidth,
        windowHeight,
        SWP_SHOWWINDOW
    );


    window.setVerticalSyncEnabled(true);
    window.setActive(true);



    // ============================================================
    // OpenGL
    // ============================================================

    glewExperimental = GL_TRUE;

    GLenum glewStatus = glewInit();

    if (glewStatus != GLEW_OK)
    {
        return 1;
    }

    glGetError();



    // ============================================================
    // Live2D
    // ============================================================

    Live2DModel mao;

    float modelScale = 1.0f;


    if (!mao.Initialize())
    {
        mao.Shutdown();
        return 1;
    }


    {
        // ============================================================
        // Motion
        // ============================================================

        Ratuber::MotionManager motionManager;

        motionManager.SetModel(
            mao.GetModel()
        );


        fs::path motionDir =
            resourceRoot / "motions";

        const char* motionIds[] =
        {
            "mtn_01", "sample_01", "mtn_02", "mtn_03", "mtn_04",
            "special_01", "special_02", "special_03"
        };

        for (const char* id : motionIds)
        {
            motionManager.LoadMotion(
                id,
                (motionDir / (std::string(id) + ".motion3.json")).string()
            );
        }



        // ============================================================
        // Expression
        // ============================================================

        Ratuber::ExpressionManager expressionManager;

        expressionManager.SetModel(
            mao.GetModel()
        );


        fs::path expressionDir =
            resourceRoot / "expressions";

        const char* expressionIds[] =
        {
            "exp_01", "exp_02", "exp_03", "exp_04",
            "exp_05", "exp_06", "exp_07", "exp_08"
        };

        for (const char* id : expressionIds)
        {
            expressionManager.LoadExpression(
                id,
                (expressionDir / (std::string(id) + ".exp3.json")).string()
            );
        }



        // ============================================================
        // Blink / Mouth Open
        // ============================================================

        Ratuber::BlinkManager blinkManager;

        blinkManager.SetModel(
            mao.GetModel()
        );


        Ratuber::MouthOpenManager mouthOpenManager;

        mouthOpenManager.SetModel(
            mao.GetModel()
        );



        // ============================================================
        // AI（文字聊天 + 语音合成）
        // ============================================================

        Mailbox mailbox;

        // ---- 聊天接口：DeepSeek。Key 从环境变量 DEEPSEEK_API_KEY 读取 ----
        Ratuber::AI::API::DeepSeekChatConfig chatConfig;

        const char* apiKey = std::getenv("DEEPSEEK_API_KEY");

        if (apiKey != nullptr)
        {
            chatConfig.apiKey = apiKey;
        }

        chatConfig.persona =
            "你是 Ratuber，一位可爱、活泼、有点小魔法的虚拟主播，"
            "正在和用户文字聊天。你说话简短自然，像朋友一样，"
            "带一点俏皮，但不啰嗦。";

        Ratuber::AI::API::DeepSeekChatAPI chatApi(chatConfig);

        Ratuber::AI::AIManager aiManager;

        aiManager.SetModel(mao.GetModel());

        aiManager.Initialize(
            &motionManager,
            &expressionManager,
            &blinkManager,
            &mouthOpenManager
        );

        aiManager.SetChatAPI(&chatApi);

        aiManager.SetErrorCallback(
            [&mailbox](const std::string& source, const std::string& message)
            {
                mailbox.PushError(source, message);
            });

        aiManager.SetReadyCallback(
            [&mailbox]
            {
                mailbox.SetReady();
            });



        // ============================================================
        // 聊天界面
        // ============================================================

        Ratuber::UI::ChatUI ui;

        // 气泡、“对话”标签、展开的面板相对头顶的距离（像素），不满意可以在这里微调：
        Ratuber::UI::ChatUIConfig uiConfig;
        // 气泡顶边离窗口顶边的距离（像素）。窗口高 1000，下移约 1/3 就是 330 左右。
        uiConfig.bubbleTop = 330.0f;
        // uiConfig.headBubbleGap = 14.0f;   // 气泡尾巴尖离头顶的空隙
        // uiConfig.headPanelRise = 30.0f;   // 展开的面板顶边比头顶高多少
        // uiConfig.headTabDrop   = 70.0f;   // “对话”标签比头顶低多少

        if (!ui.Initialize(window, uiConfig))
        {
            std::fprintf(
                stderr,
                "[UI] 找不到中文字体或输入框创建失败，气泡里可能没有文字。\n"
            );
        }

        // 气泡、标签、面板跟着角色头顶走。
        ui.SetHeadTop(headTopFor(modelScale), headCenterXFor(modelScale));

        ui.SetOnSubmit(
            [&](const std::string& text)
            {
                if (aiManager.SubmitText(text) == 0)
                {
                    ui.SetStatus("排队已满（最多 8 条），请稍等一下", true);
                }
            });

        ui.SetOnSkip(
            [&]
            {
                aiManager.SkipCurrent();
            });

        ui.SetOnCancel(
            [&](std::uint64_t id)
            {
                aiManager.CancelTask(id);
            });

        // 服务器启动完成之前不能发送。
        ui.SetInputEnabled(false);

        if (chatConfig.apiKey.empty())
        {
            ui.SetStatus(
                "未设置环境变量 DEEPSEEK_API_KEY，聊天会失败",
                true
            );
        }
        else
        {
            ui.SetStatus("正在启动服务器…", false);
        }

        aiManager.Start();

        bool serverReady = false;

        float errorShownFor = 0.0f;

        bool errorShowing = false;

        float debugTimer = 0.0f;

        bool dragging = false;

        POINT dragStartMouse{};

        POINT dragStartWindow{};

        sf::Clock frameClock;



        // ============================================================
        // 主循环
        // ============================================================

        while (window.isOpen())
        {
            // ========================================================
            // 事件
            // ========================================================

            while (const std::optional event = window.pollEvent())
            {
                if (event->is<sf::Event::Closed>())
                {
                    window.close();
                }


                if (const auto* key =
                    event->getIf<sf::Event::KeyPressed>())
                {
                    if (key->code ==
                        sf::Keyboard::Key::Escape)
                    {
                        window.close();
                    }
                }


                // 界面先处理：输入框底板、发送、跳过、右侧面板、面板里的滚轮。
                // 返回 true 就不要再当成拖动窗口或缩放模型。
                if (ui.HandleEvent(*event))
                {
                    continue;
                }



                // ====================================================
                // 鼠标缩放
                // ====================================================

                if (const auto* wheel =
                    event->getIf<sf::Event::MouseWheelScrolled>())
                {
                    if (wheel->wheel ==
                        sf::Mouse::Wheel::Vertical)
                    {
                        if (wheel->delta > 0)
                        {
                            modelScale *= 1.1f;
                        }
                        else
                        {
                            modelScale /= 1.1f;
                        }


                        modelScale =
                            std::clamp(
                                modelScale,
                                0.5f,
                                2.0f
                            );


                        mao.SetScale(modelScale);

                        // 缩放之后，气泡、标签、面板要跟着头顶重新定位。
                        ui.SetHeadTop(
                            headTopFor(modelScale),
                            headCenterXFor(modelScale));
                    }
                }



                // ====================================================
                // 鼠标拖动开始
                // ====================================================

                if (const auto* mouse =
                    event->getIf<sf::Event::MouseButtonPressed>())
                {
                    if (mouse->button ==
                        sf::Mouse::Button::Left
                        && !ui.IsPointOnUI(mouse->position))
                    {
                        dragging = true;


                        GetCursorPos(
                            &dragStartMouse
                        );


                        RECT rect{};

                        GetWindowRect(
                            hwnd,
                            &rect
                        );


                        dragStartWindow.x = rect.left;
                        dragStartWindow.y = rect.top;
                    }
                }



                // ====================================================
                // 鼠标拖动结束
                // ====================================================

                if (const auto* mouse =
                    event->getIf<sf::Event::MouseButtonReleased>())
                {
                    if (mouse->button ==
                        sf::Mouse::Button::Left)
                    {
                        dragging = false;
                    }
                }
            }



            // ========================================================
            // 窗口拖动
            // ========================================================

            if (dragging)
            {
                POINT currentMouse{};


                GetCursorPos(
                    &currentMouse
                );


                int dx =
                    currentMouse.x -
                    dragStartMouse.x;


                int dy =
                    currentMouse.y -
                    dragStartMouse.y;


                SetWindowPos(
                    hwnd,
                    HWND_TOPMOST,
                    dragStartWindow.x + dx,
                    dragStartWindow.y + dy,
                    0,
                    0,
                    SWP_NOSIZE |
                    SWP_NOACTIVATE
                );
            }



            // ========================================================
            // 时间
            // ========================================================

            // Live2D 沿用固定步长；界面和 AI 的计时用真实时间（限制上限，避免卡顿后一次跳太多）。
            const float deltaTime =
                1.0f / 60.0f;

            const float realDelta =
                std::min(frameClock.restart().asSeconds(), 0.1f);



            // ========================================================
            // AI
            // ========================================================

            {
                std::vector<std::string> errors;

                bool becameReady = false;

                {
                    std::lock_guard<std::mutex> lock(mailbox.mutex);

                    errors.swap(mailbox.errors);

                    if (mailbox.ready && !serverReady)
                    {
                        becameReady = true;
                    }
                }

                if (becameReady)
                {
                    serverReady = true;

                    ui.SetInputEnabled(true);

                    ui.SetStatus("", false);

                    ui.FocusInput();
                }

                if (!errors.empty())
                {
                    // 同一时刻有多条时只显示最后一条，详情在右侧面板的任务卡片里。
                    ui.SetStatus(errors.back(), true);

                    errorShowing = true;

                    errorShownFor = 0.0f;
                }

                if (errorShowing)
                {
                    errorShownFor += realDelta;

                    if (errorShownFor > 8.0f)
                    {
                        errorShowing = false;

                        ui.SetStatus(serverReady ? "" : "正在启动 AI 服务器…", false);
                    }
                }
            }


            aiManager.Update(realDelta);


            // AI 的播放事件 → 气泡。
            {
                Ratuber::AI::AIEvent aiEvent;

                while (aiManager.PollEvent(aiEvent))
                {
                    switch (aiEvent.type)
                    {
                    case Ratuber::AI::AIEvent::Type::SpeechStarted:
                        ui.OnSpeechStarted(aiEvent.text);
                        break;

                    case Ratuber::AI::AIEvent::Type::SegmentStarted:
                        ui.OnSegmentStarted(aiEvent.text);
                        break;

                    case Ratuber::AI::AIEvent::Type::SpeechFinished:
                        ui.OnSpeechFinished(aiEvent.text, aiEvent.skipped);
                        break;
                    }
                }
            }


            // 任务列表 → 右侧面板。
            {
                const std::vector<Ratuber::AI::TaskInfo> tasks = aiManager.GetTasks();

                std::vector<Ratuber::UI::HistoryItem> items;

                items.reserve(tasks.size());

                for (const Ratuber::AI::TaskInfo& task : tasks)
                {
                    Ratuber::UI::HistoryItem item;

                    item.id = task.id;
                    item.user = task.userText;
                    item.reply = task.replyText;
                    item.status = ToItemStatus(task.state);
                    item.note = task.note;

                    items.push_back(std::move(item));
                }

                ui.SetHistory(items);
            }


            ui.Update(realDelta);



            // ========================================================
            // OpenGL 清屏
            // ========================================================

            window.setActive(true);


            auto size =
                window.getSize();


            glViewport(
                0,
                0,
                static_cast<GLsizei>(size.x),
                static_cast<GLsizei>(size.y)
            );


            glClearColor(
                1.0f,
                0.0f,
                1.0f,
                1.0f
            );


            glClear(
                GL_COLOR_BUFFER_BIT |
                GL_DEPTH_BUFFER_BIT |
                GL_STENCIL_BUFFER_BIT
            );



            // ========================================================
            // 动作 / 表情 / 眨眼 / 嘴巴
            //
            // 待机动作（IdleBehavior）已经去掉：AI 说话时由 AIManager 里的
            // NowICanPlay 控制动作和表情，没有任务时模型保持静止。
            // ========================================================

            motionManager.Update(
                deltaTime
            );

            expressionManager.Update(
                deltaTime
            );

            blinkManager.Update(
                deltaTime
            );

            mouthOpenManager.Update(
                deltaTime
            );



            // ========================================================
            // 模型更新 + 绘制
            // ========================================================

            mao.Update(
                deltaTime
            );


            mao.Render();


            // Live2D 画完会留下一个 OpenGL 错误标志（1282）和一些绑定状态，
            // 清掉它们，免得 SFML 每帧都报告，也避免影响界面绘制。
            glBindVertexArray(0);
            glUseProgram(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            while (glGetError() != GL_NO_ERROR)
            {
            }


            // 聊天界面画在 Live2D 上面（内部保存 / 恢复 OpenGL 状态）。
            ui.Draw(window);


            window.display();



            // ========================================================
            // 调试输出
            // ========================================================

            debugTimer += deltaTime;


            if (debugTimer >= 2.0f)
            {
                debugTimer = 0.0f;


                printf(
                    "[Motion] %s  Playing=%s\n",
                    motionManager
                    .GetCurrentMotionId()
                    .c_str(),

                    motionManager.IsPlaying()
                    ?
                    "YES"
                    :
                    "NO"
                );


                printf(
                    "[Expression] %s\n",
                    expressionManager
                    .GetCurrentExpressionId()
                    .c_str()
                );
            }
        }



        // ============================================================
        // 清理
        // ============================================================

        // 先让 AI 线程停下来（它们会用到下面这些管理器）。
        aiManager.Stop();

        ui.Shutdown();

        motionManager.Clear();

        expressionManager.Clear();
    }

    mao.Shutdown();


    return 0;
}