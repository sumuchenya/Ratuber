#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "API/IChatAPI.h"
#include "NOWICANLISTEN/NowICanListen.h"
#include "NOWICANPLAY/NowICanPlay.h"
#include "NOWICANTALK/NowICanTalk.h"

// ============================================================
// AIManager.h
//
// 职责：AI 层的统一调度。
//
//   1. 打开两个服务器（ai / tts），等 /health 都正常才算就绪，
//      哪个出问题就报错指出是哪个。
//   2. 接收用户在文本框里输入的话（SubmitText），交给 NowICanListen。
//   3. 管理“周期”，保证周期之间串行。
//   4. 让模型一直眨眼、自然晃动；出声时张嘴。
//   5. 给界面提供：任务列表、取消排队、跳过当前、播放事件。
//
// ------------------------------------------------------------
// 周期
// ------------------------------------------------------------
//   从收到一句话开始，
//   到这句话对应的 AI 回复的音频播完、动作 / 表情也表达完，
//   算一个周期。周期之间只能串行。
//
//   在上一个周期还没结束时又收到一句话：
//   立刻去请求 AI、提交 TTS、请求 /decide（不浪费时间），
//   但发给 ai_server 的 busy 改成 true（只会得到表情），
//   结果排队，等上一个周期结束再表现出来。
//
// ------------------------------------------------------------
// 任务（TaskInfo）：一句用户输入 = 一个任务 = 一个周期
// ------------------------------------------------------------
//   Waiting    排队中，还没去请求 AI           可取消
//   Thinking   正在请求 AI / 提交 TTS          可取消
//   Ready      AI 已回复，等上一个周期结束     可取消
//   Speaking   正在播放（当前周期）            可跳过
//   Done / Skipped / Cancelled / Failed        已结束
//
// ------------------------------------------------------------
// 线程
// ------------------------------------------------------------
//   SetModel / Initialize / Update / PollEvent   主线程。
//   其余公开函数                                  任意线程（界面线程即可）。
//   错误回调、就绪回调                            在工作线程里被调用。
//
// ------------------------------------------------------------
// 界面怎么用（每帧）：
//
//   aiManager.Update(dt);
//
//   AIEvent e;
//   while (aiManager.PollEvent(e)) { ...气泡的文字逐句显现... }
//
//   auto tasks = aiManager.GetTasks();       // 画排队列表 / 历史
//
//   用户回车：     aiManager.SubmitText(text);
//   点取消：       aiManager.CancelTask(id);
//   点跳过：       aiManager.SkipCurrent();
// ============================================================

namespace Live2D
{
    namespace Cubism
    {
        namespace Framework
        {
            class CubismModel;
        }
    }
}

namespace Ratuber
{
    class MotionManager;
    class ExpressionManager;
    class BlinkManager;
    class MouthOpenManager;

    class Blink;
    class Mouth;
    class Shake;
}

namespace Ratuber::AI
{
    // 一个服务器怎么启动、怎么检查。
    struct ServerSpec
    {
        // 报错时显示的名字。
        std::string name;

        int port = 0;

        // 这个服务器用的 python.exe（conda 环境就写环境里的那个）。
        std::wstring pythonExe;

        std::wstring script;

        // 等它就绪的最长时间（模型加载比较慢）。
        int readyTimeoutSec = 60;
    };

    struct AIManagerConfig
    {
        // 例如 D:\Ratuber\Ratuber
        std::wstring projectDir;

        // 服务器脚本的工作目录：<projectDir>\AI\Server
        std::wstring workDir;

        // tts_server.py 存放 WAV 的目录：<projectDir>\tempvoice
        std::wstring tempVoiceDir;

        ServerSpec ai;
        ServerSpec tts;

        // false：不由程序启动服务器（你已经手动开好了），只等它们 /health 正常。
        // 某个服务器的 /health 已经正常时，即使是 true 也不会重复启动它。
        bool launchServers = true;

        // true：每个服务器开一个自己的控制台窗口，能看到它们的日志和报错。
        bool showServerConsole = true;

        // 排队等 AI 的话最多多少句，超过的直接拒绝。
        int maxPendingUtterances = 8;

        // 一个周期正常结束、后面没有排队的任务时，让模型回到待机状态。
        // 如果你观察到动作演完后模型会自己回到站立，保持 false 即可。
        bool returnToRestAfterCycle = false;

        // 默认值：项目路径取 CMake 传进来的 RATUBER_PROJECT_DIR，
        // python 路径是 conda 里两个环境的 python.exe。
        static AIManagerConfig Default();
    };


    // ============================================================
    // 任务
    // ============================================================

    enum class TaskState
    {
        Waiting,
        Thinking,
        Ready,
        Speaking,
        Done,
        Skipped,
        Cancelled,
        Failed,
    };

    struct TaskInfo
    {
        // 从 1 开始，0 表示无效。
        std::uint64_t id = 0;

        // 用户说的话。
        std::string userText;

        // AI 的回复。AI 还没回复时为空。
        std::string replyText;

        std::string emotion;

        TaskState state = TaskState::Waiting;

        // Failed / Done 时的补充说明（例如错误原因）。
        std::string note;
    };

    // 播放事件，界面用它让气泡里的文字跟着声音走。
    struct AIEvent
    {
        enum class Type
        {
            // 这个任务轮到播放了。text = AI 的完整回复。
            // 声音可能还要等一会儿才出来（TTS 还在生成）。
            SpeechStarted,

            // 一个小句开始播放。text = 这个小句的文字。
            SegmentStarted,

            // 这个任务播放结束（正常结束 / 被跳过 / 语音没生成出来）。
            // text = AI 的完整回复。界面应当把没显现完的文字补完。
            SpeechFinished,
        };

        Type type = Type::SpeechStarted;

        std::uint64_t taskId = 0;

        std::string text;

        int segmentIndex = 0;

        int segmentCount = 0;

        // SpeechFinished：是否被用户跳过。
        bool skipped = false;
    };


    class AIManager
    {
    public:
        // 错误回调：source 是哪个环节出的问题（例如 "TTS 服务器"）。
        using ErrorCallback =
            std::function<void(const std::string& source,
                               const std::string& message)>;

        // 两个服务器都就绪之后调用一次。
        using ReadyCallback = std::function<void()>;

        explicit AIManager(
            AIManagerConfig config = AIManagerConfig::Default()
        );

        ~AIManager();

        AIManager(const AIManager&) = delete;
        AIManager& operator=(const AIManager&) = delete;


        // ---------------- 和 IdleBehavior 一样的接入方式 ----------------

        void SetModel(
            Live2D::Cubism::Framework::CubismModel* model
        );

        void Initialize(
            MotionManager* motionManager,
            ExpressionManager* expressionManager,
            BlinkManager* blinkManager,
            MouthOpenManager* mouthOpenManager
        );

        // 每帧调用一次，不会阻塞。
        void Update(float deltaTime);


        // ---------------- 服务与对话 ----------------

        // 对话用的 AI。不拥有，调用方保证它活得比本对象久。
        void SetChatAPI(API::IChatAPI* chatApi);

        void SetErrorCallback(ErrorCallback callback);

        void SetReadyCallback(ReadyCallback callback);

        // 在后台启动两个服务器并等待就绪，立刻返回。
        void Start();

        // 停止所有工作线程。两个服务器进程不会被关闭。
        void Stop();

        // 两个服务器是否都已就绪。
        bool IsReady() const
        {
            return m_ready.load();
        }

        // 是否还有没结束的周期（正在播放，或者排着队）。
        bool HasUnfinishedCycle() const;


        // ---------------- 输入 ----------------

        // 用户在文本框里输入的话。返回任务 id；
        // 返回 0 表示没有收下（文本为空、服务器没就绪、排队已满）。
        std::uint64_t SubmitText(const std::string& text);

        // 同上，但参数是 {"status":"ok","text":"..."} 形式的 JSON。
        std::uint64_t SubmitUtteranceJson(const std::string& json);


        // ---------------- 任务控制 ----------------

        // 取消一个还没轮到播放的任务（Waiting / Thinking / Ready）。
        // 已经在播放的请用 SkipCurrent。返回 false 表示不能取消。
        bool CancelTask(std::uint64_t taskId);

        // 跳过正在播放的任务：声音和动作立刻停止，模型回到待机状态，
        // 文字由界面补完，然后直接进入下一个任务。没有在播放的任务时返回 false。
        bool SkipCurrent();

        // 正在播放的任务 id，没有时为 0。
        std::uint64_t CurrentTaskId() const;

        // 所有任务的快照（按 id 从小到大）。最多保留最近 200 个。
        std::vector<TaskInfo> GetTasks() const;

        // 取一个播放事件。主线程每帧循环调用，直到返回 false。
        bool PollEvent(AIEvent& event);

    private:
        struct PendingUtterance
        {
            std::uint64_t seq = 0;

            std::string json;
        };

        struct PreparedCycle
        {
            std::uint64_t seq = 0;

            ListenResult result;
        };

        // 后台善后：丢弃被取消任务的语音文件、把那一轮从对话历史里撤掉。
        struct CleanupItem
        {
            std::string ttsJson;

            std::string forgetUserText;
        };

        void ReportError(
            const std::string& source,
            const std::string& message
        );

        std::uint64_t Enqueue(
            const std::string& json,
            const std::string& userText
        );

        // 以下带 Locked 的函数调用时必须已经持有 m_mutex。
        TaskInfo* FindTaskLocked(std::uint64_t id);
        void SetStateLocked(std::uint64_t id, TaskState state, const std::string& note = "");
        void PruneTasksLocked();
        void PushEventLocked(AIEvent event);
        bool HasEarlierUnfinishedLocked(std::uint64_t seq) const;

        // ---- 启动服务器 ----
        void StartupLoop();

        // ---- 周期 ----
        void PrepareLoop();
        void PerformLoop();
        void PerformCycle(const PreparedCycle& cycle);
        void FinishCycle(std::uint64_t seq, TaskState state, const std::string& note);
        bool HasEarlierUnfinishedCycle(std::uint64_t seq) const;
        void CleanupLoop();

        AIManagerConfig m_config;

        NowICanListen m_listen;
        NowICanTalk m_talk;
        NowICanPlay m_play;

        API::IChatAPI* m_chatApi = nullptr;

        // ---- 主线程 ----
        Live2D::Cubism::Framework::CubismModel* m_model = nullptr;
        MotionManager* m_motionManager = nullptr;
        BlinkManager* m_blinkManager = nullptr;
        MouthOpenManager* m_mouthOpenManager = nullptr;

        std::unique_ptr<Blink> m_blink;
        std::unique_ptr<Mouth> m_mouth;
        std::unique_ptr<Shake> m_shake;

        bool m_shakeReady = false;
        bool m_initialized = false;
        bool m_wasSpeaking = false;

        // ---- 线程与队列 ----
        std::atomic<bool> m_started{ false };
        std::atomic<bool> m_stopping{ false };
        std::atomic<bool> m_ready{ false };

        // 当前周期是否被跳过（Stop 时也会置位，让等待立刻结束）。
        std::atomic<bool> m_skip{ false };

        std::thread m_startupThread;
        std::thread m_prepareThread;
        std::thread m_performThread;
        std::thread m_cleanupThread;

        mutable std::mutex m_mutex;
        std::condition_variable m_condition;

        // 等着请求 AI 的话。
        std::deque<PendingUtterance> m_pending;

        // 已经准备好（AI 回复了、两个服务器都响应了），等着表现的周期。
        std::deque<PreparedCycle> m_prepared;

        // 还没结束的周期序号。
        std::set<std::uint64_t> m_unfinished;

        // 正在请求 AI 的那一个（0 表示没有）。
        std::uint64_t m_preparingSeq = 0;

        // 在请求 AI 的过程中被取消的任务，请求回来后丢弃结果。
        std::set<std::uint64_t> m_cancelled;

        // 正在播放的那一个（0 表示没有）。
        std::uint64_t m_currentSeq = 0;

        std::map<std::uint64_t, TaskInfo> m_tasks;

        std::deque<AIEvent> m_events;

        std::deque<CleanupItem> m_cleanup;

        std::uint64_t m_nextSeq = 1;

        ErrorCallback m_errorCallback;
        ReadyCallback m_readyCallback;
    };
}
