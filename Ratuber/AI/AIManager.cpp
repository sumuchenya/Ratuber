#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include "AIManager.h"

#include <algorithm>
#include <chrono>
#include <vector>

#include <nlohmann/json.hpp>

#include "AIPlatform.h"
#include "AIProtocol.h"

#include "behavior/SpecialBehavior/blink.h"
#include "behavior/SpecialBehavior/mouth.h"
#include "behavior/SpecialBehavior/shake.h"

#include "motion/MotionManager.h"
#include "blink/BlinkManager.h"
#include "MouthOpen/MouthOpenManager.h"

#ifndef RATUBER_PROJECT_DIR
#define RATUBER_PROJECT_DIR "D:/Ratuber/Ratuber"
#endif

namespace Ratuber::AI
{
    namespace
    {
        constexpr const char* kTag = "AIManager";

        using Clock = std::chrono::steady_clock;

        std::wstring FixSlashes(std::wstring path)
        {
            std::replace(path.begin(), path.end(), L'/', L'\\');

            while (path.size() > 3 && path.back() == L'\\')
            {
                path.pop_back();
            }

            return path;
        }

        std::wstring DirectoryOf(const std::wstring& file)
        {
            const size_t pos = file.find_last_of(L"\\/");

            return pos == std::wstring::npos ? L"" : file.substr(0, pos);
        }

        // /health 返回 {"status":"ok"...} 或 {"ok":true...} 才算就绪。
        // （tts_server 在模型还没加载好时返回 ok=false。）
        bool HealthOk(int port)
        {
            const Platform::Http::Response response =
                Platform::Http::Get(port, "/health", 1500);

            if (!response.Ok())
            {
                return false;
            }

            const nlohmann::json json =
                nlohmann::json::parse(response.body, nullptr, false);

            if (json.is_discarded() || !json.is_object())
            {
                return false;
            }

            if (json.contains("ok") && json["ok"].is_boolean())
            {
                return json["ok"].get<bool>();
            }

            if (json.contains("status") && json["status"].is_string())
            {
                return json["status"].get<std::string>() == "ok";
            }

            return false;
        }

        // 子进程的环境变量：当前环境 + conda 环境的 PATH + UTF-8。
        // 没有这些 PATH，conda 环境里的 DLL（torch、ffmpeg 等）可能找不到。
        std::vector<wchar_t> BuildEnvironment(const std::wstring& pythonExe)
        {
            const std::wstring dir = DirectoryOf(pythonExe);

            const std::wstring extraPath =
                dir + L";"
                + dir + L"\\Library\\mingw-w64\\bin;"
                + dir + L"\\Library\\usr\\bin;"
                + dir + L"\\Library\\bin;"
                + dir + L"\\Scripts;"
                + dir + L"\\bin;";

            std::vector<std::wstring> entries;

            wchar_t* block = GetEnvironmentStringsW();

            if (block != nullptr)
            {
                for (const wchar_t* p = block; *p != L'\0';)
                {
                    const std::wstring entry = p;

                    p += entry.size() + 1;

                    // 以 = 开头的是 "=C:=C:\..." 这类内部变量，原样保留。
                    entries.push_back(entry);
                }

                FreeEnvironmentStringsW(block);
            }

            bool pathDone = false;

            for (std::wstring& entry : entries)
            {
                if (entry.size() > 5
                    && _wcsnicmp(entry.c_str(), L"PATH=", 5) == 0)
                {
                    entry = L"PATH=" + extraPath + entry.substr(5);

                    pathDone = true;
                }
            }

            if (!pathDone)
            {
                entries.push_back(L"PATH=" + extraPath);
            }

            entries.push_back(L"PYTHONUTF8=1");
            entries.push_back(L"PYTHONIOENCODING=utf-8");

            std::vector<wchar_t> result;

            for (const std::wstring& entry : entries)
            {
                result.insert(result.end(), entry.begin(), entry.end());
                result.push_back(L'\0');
            }

            result.push_back(L'\0');

            return result;
        }

    }


    // ============================================================
    // 默认配置
    // ============================================================

    AIManagerConfig AIManagerConfig::Default()
    {
        AIManagerConfig config;

        config.projectDir = FixSlashes(
            Platform::Utf8ToWide(RATUBER_PROJECT_DIR)
        );

        config.workDir = config.projectDir + L"\\AI\\Server";
        config.tempVoiceDir = config.projectDir + L"\\tempvoice";

        config.ai.name = "AI 服务器";
        config.ai.port = 8001;
        config.ai.pythonExe = L"C:\\Users\\user\\miniconda3\\python.exe";
        config.ai.script = L"ai_server.py";
        config.ai.readyTimeoutSec = 60;

        config.tts.name = "TTS 服务器";
        config.tts.port = 8000;
        config.tts.pythonExe =
            L"C:\\Users\\user\\miniconda3\\envs\\cosyvoice\\python.exe";
        config.tts.script = L"tts_server.py";
        config.tts.readyTimeoutSec = 300;

        return config;
    }


    // ============================================================
    // 构造 / 析构
    // ============================================================

    namespace
    {
        ListenConfig MakeListenConfig(const AIManagerConfig& config)
        {
            ListenConfig listen;

            listen.ttsPort = config.tts.port;
            listen.aiPort = config.ai.port;

            return listen;
        }

        TalkConfig MakeTalkConfig(const AIManagerConfig& config)
        {
            TalkConfig talk;

            talk.ttsPort = config.tts.port;
            talk.tempVoiceDir = config.tempVoiceDir;

            return talk;
        }
    }

    AIManager::AIManager(AIManagerConfig config)
        :
        m_config(std::move(config)),
        m_listen(MakeListenConfig(m_config)),
        m_talk(MakeTalkConfig(m_config))
    {}

    AIManager::~AIManager()
    {
        Stop();
    }


    // ============================================================
    // 主线程接口（和 IdleBehavior 一样的接入方式）
    // ============================================================

    void AIManager::SetModel(
        Live2D::Cubism::Framework::CubismModel* model
    )
    {
        m_model = model;
        m_shakeReady = false;
    }

    void AIManager::Initialize(
        MotionManager* motionManager,
        ExpressionManager* expressionManager,
        BlinkManager* blinkManager,
        MouthOpenManager* mouthOpenManager
    )
    {
        m_motionManager = motionManager;
        m_blinkManager = blinkManager;
        m_mouthOpenManager = mouthOpenManager;

        m_shakeReady = false;

        m_blink = std::make_unique<Blink>();
        m_mouth = std::make_unique<Mouth>();
        m_shake = std::make_unique<Shake>();

        if (m_blinkManager)
        {
            m_blink->Initialize(m_blinkManager);
        }

        if (m_mouthOpenManager)
        {
            m_mouth->Initialize(m_mouthOpenManager);
        }

        m_play.Initialize(motionManager, expressionManager);

        m_initialized = true;
    }

    void AIManager::Update(float deltaTime)
    {
        if (!m_initialized)
        {
            return;
        }

        // 动作 / 表情（只有这里会碰模型，所以必须在主线程）。
        m_play.Update(deltaTime);

        // 晃动：main.cpp 先 Initialize 后 SetModel，所以等模型到位再初始化。
        if (m_model)
        {
            if (!m_shakeReady)
            {
                m_shake->Initialize(m_model);

                m_shakeReady = true;
            }

            // 动作播放期间不调用晃动，动作自带头部和身体运动。
            const bool motionPlaying =
                m_motionManager && m_motionManager->IsPlaying();

            if (!motionPlaying)
            {
                m_shake->Update(deltaTime);
            }
        }

        // 眨眼：一直运行。
        if (m_blinkManager)
        {
            m_blink->Update(deltaTime);
        }

        // 张嘴：只在声音播放期间。
        // 声音一停，Mouth 就不再被调用，嘴会停在最后张开的那一刻，
        // 所以由说话变为不说话的这一帧要主动闭嘴。
        const bool speaking = m_talk.IsSpeaking();

        if (m_mouthOpenManager)
        {
            if (speaking)
            {
                m_mouth->Update(deltaTime);
            }
            else if (m_wasSpeaking)
            {
                m_mouthOpenManager->CloseMouth();
            }
        }

        m_wasSpeaking = speaking;
    }


    // ============================================================
    // 设置
    // ============================================================

    void AIManager::SetChatAPI(API::IChatAPI* chatApi)
    {
        m_chatApi = chatApi;

        m_listen.SetChatAPI(chatApi);
    }

    void AIManager::SetErrorCallback(ErrorCallback callback)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_errorCallback = std::move(callback);
    }

    void AIManager::SetReadyCallback(ReadyCallback callback)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_readyCallback = std::move(callback);
    }

    void AIManager::ReportError(
        const std::string& source,
        const std::string& message
    )
    {
        Platform::Log(kTag, "【错误】" + source + "：" + message);

        ErrorCallback callback;

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            callback = m_errorCallback;
        }

        if (callback)
        {
            callback(source, message);
        }
    }


    // ============================================================
    // 启动 / 停止
    // ============================================================

    void AIManager::Start()
    {
        if (m_started.exchange(true))
        {
            return;
        }

        m_startupThread = std::thread([this] { StartupLoop(); });
        m_prepareThread = std::thread([this] { PrepareLoop(); });
        m_performThread = std::thread([this] { PerformLoop(); });
        m_cleanupThread = std::thread([this] { CleanupLoop(); });
    }

    void AIManager::Stop()
    {
        if (!m_started.load())
        {
            return;
        }

        m_stopping = true;
        m_skip = true;

        // 打断正在播放的声音、让 Play 的等待立刻结束。
        m_talk.Stop();
        m_play.Abort();

        // 先拿一下锁再通知，避免工作线程刚检查完条件、还没开始等待时错过通知。
        {
            std::lock_guard<std::mutex> lock(m_mutex);
        }

        m_condition.notify_all();

        if (m_startupThread.joinable())
        {
            m_startupThread.join();
        }

        if (m_prepareThread.joinable())
        {
            m_prepareThread.join();
        }

        if (m_performThread.joinable())
        {
            m_performThread.join();
        }

        if (m_cleanupThread.joinable())
        {
            m_cleanupThread.join();
        }

        // 注意：服务器进程不关闭。
    }

    bool AIManager::HasUnfinishedCycle() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        return !m_unfinished.empty();
    }

    bool AIManager::HasEarlierUnfinishedLocked(std::uint64_t seq) const
    {
        return !m_unfinished.empty() && *m_unfinished.begin() < seq;
    }

    bool AIManager::HasEarlierUnfinishedCycle(std::uint64_t seq) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        return HasEarlierUnfinishedLocked(seq);
    }


    // ============================================================
    // 启动三个服务器并等待就绪
    // ============================================================

    void AIManager::StartupLoop()
    {
        struct Server
        {
            const ServerSpec* spec = nullptr;

            HANDLE process = nullptr;

            bool ready = false;

            bool failed = false;

            Clock::time_point deadline;
        };

        Server servers[2];

        servers[0].spec = &m_config.ai;
        servers[1].spec = &m_config.tts;

        const auto begin = Clock::now();

        // ---- 1. 启动 ----

        for (Server& server : servers)
        {
            const ServerSpec& spec = *server.spec;

            server.deadline = begin + std::chrono::seconds(spec.readyTimeoutSec);

            if (HealthOk(spec.port))
            {
                Platform::Log(kTag, spec.name + " 已经在运行（端口 "
                    + std::to_string(spec.port) + "），不再重复启动");

                server.ready = true;

                continue;
            }

            if (!m_config.launchServers)
            {
                Platform::Log(kTag, spec.name + " 还没有响应，等待你手动启动它（端口 "
                    + std::to_string(spec.port) + "）");

                continue;
            }

            const std::wstring scriptPath = m_config.workDir + L"\\" + spec.script;

            if (!Platform::FileExists(spec.pythonExe))
            {
                ReportError(spec.name, "找不到 python：" + Platform::WideToUtf8(spec.pythonExe));

                server.failed = true;

                continue;
            }

            if (!Platform::FileExists(scriptPath))
            {
                ReportError(spec.name, "找不到脚本：" + Platform::WideToUtf8(scriptPath));

                server.failed = true;

                continue;
            }

            std::wstring commandLine =
                L"\"" + spec.pythonExe + L"\" -u \"" + scriptPath + L"\"";

            std::vector<wchar_t> environment = BuildEnvironment(spec.pythonExe);

            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);

            PROCESS_INFORMATION info{};

            const DWORD flags =
                CREATE_UNICODE_ENVIRONMENT
                | (m_config.showServerConsole
                    ? CREATE_NEW_CONSOLE
                    : (CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP));

            // CreateProcessW 要求命令行缓冲区可写。
            std::vector<wchar_t> buffer(commandLine.begin(), commandLine.end());
            buffer.push_back(L'\0');

            const BOOL created = CreateProcessW(
                nullptr,
                buffer.data(),
                nullptr,
                nullptr,
                FALSE,
                flags,
                environment.data(),
                m_config.workDir.c_str(),
                &startup,
                &info
            );

            if (!created)
            {
                ReportError(spec.name,
                    "启动失败（CreateProcess 错误码 "
                    + std::to_string(GetLastError()) + "）：" + Platform::WideToUtf8(commandLine));

                server.failed = true;

                continue;
            }

            CloseHandle(info.hThread);

            server.process = info.hProcess;

            Platform::Log(kTag, "已启动 " + spec.name + "（端口 "
                + std::to_string(spec.port) + "），等待就绪……");
        }

        // ---- 2. 等 /health ----

        for (;;)
        {
            if (m_stopping)
            {
                break;
            }

            bool allDone = true;

            for (Server& server : servers)
            {
                if (server.ready || server.failed)
                {
                    continue;
                }

                const ServerSpec& spec = *server.spec;

                if (HealthOk(spec.port))
                {
                    server.ready = true;

                    Platform::Log(kTag, spec.name + " 已就绪（用时 "
                        + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                            Clock::now() - begin).count()) + " 秒）");

                    continue;
                }

                // 进程已经退出：几乎一定是脚本报错了。
                if (server.process != nullptr)
                {
                    DWORD exitCode = 0;

                    if (GetExitCodeProcess(server.process, &exitCode)
                        && exitCode != STILL_ACTIVE)
                    {
                        ReportError(spec.name,
                            "进程已退出（退出码 " + std::to_string(exitCode)
                            + "）。看一下它的控制台窗口里的报错"
                            + "（showServerConsole 要为 true）");

                        server.failed = true;

                        continue;
                    }
                }

                if (Clock::now() >= server.deadline)
                {
                    ReportError(spec.name,
                        "等了 " + std::to_string(spec.readyTimeoutSec)
                        + " 秒仍然没有就绪（端口 " + std::to_string(spec.port)
                        + "，/health 没有返回正常）");

                    server.failed = true;

                    continue;
                }

                allDone = false;
            }

            if (allDone)
            {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // 不关闭进程，只释放句柄。
        for (Server& server : servers)
        {
            if (server.process != nullptr)
            {
                CloseHandle(server.process);

                server.process = nullptr;
            }
        }

        if (m_stopping)
        {
            return;
        }

        const bool allReady = servers[0].ready && servers[1].ready;

        if (!allReady)
        {
            std::string names;

            for (const Server& server : servers)
            {
                if (!server.ready)
                {
                    names += (names.empty() ? "" : "、") + server.spec->name;
                }
            }

            ReportError("服务器", "没有全部就绪，未就绪的有：" + names);

            return;
        }

        m_ready = true;

        Platform::Log(kTag, "两个服务器都已就绪，可以开始对话");

        ReadyCallback callback;

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            callback = m_readyCallback;
        }

        if (callback)
        {
            callback();
        }
    }


    // ============================================================
    // 任务表
    // ============================================================

    namespace
    {
        constexpr size_t kMaxTasks = 200;

        constexpr size_t kMaxEvents = 512;

        bool IsFinishedState(TaskState state)
        {
            return state == TaskState::Done
                || state == TaskState::Skipped
                || state == TaskState::Cancelled
                || state == TaskState::Failed;
        }
    }

    TaskInfo* AIManager::FindTaskLocked(std::uint64_t id)
    {
        const auto it = m_tasks.find(id);

        return it == m_tasks.end() ? nullptr : &it->second;
    }

    void AIManager::SetStateLocked(
        std::uint64_t id,
        TaskState state,
        const std::string& note
    )
    {
        if (TaskInfo* task = FindTaskLocked(id))
        {
            task->state = state;

            if (!note.empty())
            {
                task->note = note;
            }
        }
    }

    void AIManager::PruneTasksLocked()
    {
        // 只删最老的、已经结束的；碰到没结束的就停。
        while (m_tasks.size() > kMaxTasks)
        {
            const auto it = m_tasks.begin();

            if (!IsFinishedState(it->second.state))
            {
                break;
            }

            m_tasks.erase(it);
        }
    }

    void AIManager::PushEventLocked(AIEvent event)
    {
        m_events.push_back(std::move(event));

        while (m_events.size() > kMaxEvents)
        {
            m_events.pop_front();
        }
    }

    std::vector<TaskInfo> AIManager::GetTasks() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<TaskInfo> tasks;

        tasks.reserve(m_tasks.size());

        for (const auto& entry : m_tasks)
        {
            tasks.push_back(entry.second);
        }

        return tasks;
    }

    bool AIManager::PollEvent(AIEvent& event)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_events.empty())
        {
            return false;
        }

        event = std::move(m_events.front());

        m_events.pop_front();

        return true;
    }

    std::uint64_t AIManager::CurrentTaskId() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        return m_currentSeq;
    }


    // ============================================================
    // 输入
    // ============================================================

    std::uint64_t AIManager::Enqueue(
        const std::string& json,
        const std::string& userText
    )
    {
        if (!m_ready.load())
        {
            Platform::Log(kTag, "服务器还没就绪，没有收下这句话");

            return 0;
        }

        std::uint64_t id = 0;

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (static_cast<int>(m_pending.size()) >= m_config.maxPendingUtterances)
            {
                Platform::Log(kTag, "排队的话太多，没有收下这一句");

                return 0;
            }

            id = m_nextSeq++;

            TaskInfo task;

            task.id = id;
            task.userText = userText;

            m_tasks[id] = std::move(task);

            // 从收到这句话起，这个周期就算开始了。
            m_unfinished.insert(id);

            PendingUtterance item;

            item.seq = id;
            item.json = json;

            m_pending.push_back(std::move(item));

            PruneTasksLocked();
        }

        m_condition.notify_all();

        return id;
    }

    std::uint64_t AIManager::SubmitText(const std::string& text)
    {
        const std::string trimmed = Protocol::Trim(text);

        if (trimmed.empty())
        {
            return 0;
        }

        nlohmann::json json;

        json["status"] = "ok";
        json["text"] = trimmed;

        return Enqueue(
            json.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace),
            trimmed
        );
    }

    std::uint64_t AIManager::SubmitUtteranceJson(const std::string& json)
    {
        Protocol::Utterance utterance;
        std::string error;

        if (!Protocol::ParseUtterance(json, utterance, error))
        {
            ReportError("输入", error);

            return 0;
        }

        std::string userText = Protocol::Trim(utterance.text);

        if (utterance.status == "ok" && userText.empty())
        {
            return 0;
        }

        if (utterance.status == "too_long")
        {
            userText = "（说得太久了）";
        }

        return Enqueue(json, userText);
    }


    // ============================================================
    // 取消 / 跳过
    // ============================================================

    bool AIManager::CancelTask(std::uint64_t taskId)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            TaskInfo* task = FindTaskLocked(taskId);

            if (task == nullptr)
            {
                return false;
            }

            switch (task->state)
            {
            case TaskState::Waiting:
            {
                const auto it = std::find_if(
                    m_pending.begin(), m_pending.end(),
                    [taskId](const PendingUtterance& p) { return p.seq == taskId; });

                if (it == m_pending.end())
                {
                    return false;
                }

                m_pending.erase(it);

                break;
            }

            case TaskState::Thinking:
            {
                // 请求已经发出去了，没法真正中断，等结果回来再丢掉。
                if (taskId != m_preparingSeq)
                {
                    return false;
                }

                m_cancelled.insert(taskId);

                break;
            }

            case TaskState::Ready:
            {
                const auto it = std::find_if(
                    m_prepared.begin(), m_prepared.end(),
                    [taskId](const PreparedCycle& c) { return c.seq == taskId; });

                if (it == m_prepared.end())
                {
                    return false;
                }

                CleanupItem cleanup;

                cleanup.ttsJson = it->result.ttsResponseJson;

                if (!it->result.replyText.empty())
                {
                    cleanup.forgetUserText = task->userText;
                }

                if (!cleanup.ttsJson.empty() || !cleanup.forgetUserText.empty())
                {
                    m_cleanup.push_back(std::move(cleanup));
                }

                m_prepared.erase(it);

                break;
            }

            default:
                return false;
            }

            SetStateLocked(taskId, TaskState::Cancelled, "已取消");

            m_unfinished.erase(taskId);

            PruneTasksLocked();
        }

        Platform::Log(kTag, "任务 " + std::to_string(taskId) + " 已取消");

        m_condition.notify_all();

        return true;
    }

    bool AIManager::SkipCurrent()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_currentSeq == 0)
            {
                return false;
            }

            m_skip = true;

            // 声音立刻停；动作和表情在主线程的下一次 Update 里停下并回到待机。
            m_talk.Interrupt();

            m_play.RequestStop(true);
        }

        Platform::Log(kTag, "跳过当前播放");

        return true;
    }


    // ============================================================
    // 周期
    // ============================================================

    void AIManager::FinishCycle(
        std::uint64_t seq,
        TaskState state,
        const std::string& note
    )
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_unfinished.erase(seq);

            SetStateLocked(seq, state, note);

            PruneTasksLocked();
        }

        Platform::Log(kTag, "周期 " + std::to_string(seq) + " 结束"
            + (note.empty() ? std::string() : "：" + note));

        m_condition.notify_all();
    }

    // 准备：请求 AI、提交 TTS、请求 /decide。
    // 不等前一个周期，所以排队的话不会浪费时间；
    // 前一个周期没结束时 busy 会被置为 true（由 NowICanListen 在发送前一刻调用回调判断）。
    void AIManager::PrepareLoop()
    {
        while (!m_stopping)
        {
            PendingUtterance item;

            {
                std::unique_lock<std::mutex> lock(m_mutex);

                m_condition.wait(lock, [this]
                {
                    return m_stopping.load() || !m_pending.empty();
                });

                if (m_stopping)
                {
                    return;
                }

                item = std::move(m_pending.front());

                m_pending.pop_front();

                m_preparingSeq = item.seq;

                SetStateLocked(item.seq, TaskState::Thinking);
            }

            const std::uint64_t seq = item.seq;

            ListenResult result = m_listen.Process(
                item.json,
                [this, seq] { return HasEarlierUnfinishedCycle(seq); }
            );

            bool cancelled = false;

            std::string userText;

            {
                std::lock_guard<std::mutex> lock(m_mutex);

                m_preparingSeq = 0;

                cancelled = m_cancelled.erase(seq) > 0;

                if (const TaskInfo* task = FindTaskLocked(seq))
                {
                    userText = task->userText;
                }
            }

            // 处理过程中被用户取消：丢掉结果，并把这一轮从对话历史里撤掉。
            // 在这个线程里直接撤，下一次请求 AI 之前就已经生效。
            if (cancelled)
            {
                if (!result.replyText.empty() && m_chatApi != nullptr)
                {
                    m_chatApi->ForgetExchange(userText);
                }

                if (!result.ttsResponseJson.empty())
                {
                    std::lock_guard<std::mutex> lock(m_mutex);

                    CleanupItem cleanup;

                    cleanup.ttsJson = result.ttsResponseJson;

                    m_cleanup.push_back(std::move(cleanup));
                }

                m_condition.notify_all();

                continue;
            }

            for (const ListenError& error : result.errors)
            {
                ReportError(error.source, error.message);
            }

            // 文本聊天：只要 AI 回复了文字，就一定要让用户看到，
            // 即使语音和动作都没有生成出来。
            const bool hasContent =
                result.hasSpeech
                || result.hasActions
                || !result.replyText.empty();

            if (!hasContent)
            {
                if (!result.errors.empty())
                {
                    FinishCycle(seq, TaskState::Failed, result.errors.front().message);
                }
                else
                {
                    FinishCycle(seq, TaskState::Done, result.skippedReason);
                }

                continue;
            }

            {
                std::lock_guard<std::mutex> lock(m_mutex);

                if (TaskInfo* task = FindTaskLocked(seq))
                {
                    task->replyText = result.replyText;
                    task->emotion = result.emotion;
                }

                SetStateLocked(seq, TaskState::Ready);

                PreparedCycle cycle;

                cycle.seq = seq;
                cycle.result = std::move(result);

                // 准备线程只有一个，所以 m_prepared 天然按 seq 排序。
                m_prepared.push_back(std::move(cycle));
            }

            m_condition.notify_all();
        }
    }

    // 表现：严格一个周期接一个周期。
    void AIManager::PerformLoop()
    {
        while (!m_stopping)
        {
            PreparedCycle cycle;

            {
                std::unique_lock<std::mutex> lock(m_mutex);

                m_condition.wait(lock, [this]
                {
                    return m_stopping.load() || !m_prepared.empty();
                });

                if (m_stopping)
                {
                    return;
                }

                cycle = std::move(m_prepared.front());

                m_prepared.pop_front();
            }

            PerformCycle(cycle);
        }
    }

    void AIManager::PerformCycle(const PreparedCycle& cycle)
    {
        const ListenResult& result = cycle.result;

        const std::uint64_t seq = cycle.seq;

        const std::string& replyText = result.replyText;

        // 开始这个周期。清标志、置当前任务要和 SkipCurrent 互斥，
        // 否则“跳过”可能被这里的清标志吞掉。
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_currentSeq = seq;

            m_skip = m_stopping.load();

            m_talk.ClearInterrupt();

            SetStateLocked(seq, TaskState::Speaking);

            AIEvent started;

            started.type = AIEvent::Type::SpeechStarted;
            started.taskId = seq;
            started.text = replyText;

            PushEventLocked(std::move(started));
        }

        bool actionsSubmitted = false;

        auto submitActions = [&]
        {
            if (actionsSubmitted || m_skip.load())
            {
                return;
            }

            actionsSubmitted = true;

            if (result.hasActions)
            {
                if (!m_play.Submit(result.decideResponseJson))
                {
                    ReportError("动作表情", "ai_server 的输出无法解析");
                }
            }
        };

        std::string note;

        if (result.hasSpeech)
        {
            // 动作 / 表情在第一个声音响起的那一刻同时开始。
            const TalkResult talk = m_talk.Play(
                result.ttsResponseJson,
                submitActions,
                [this, seq](int index, int count, const std::string& text)
                {
                    AIEvent segment;

                    segment.type = AIEvent::Type::SegmentStarted;
                    segment.taskId = seq;
                    segment.text = text;
                    segment.segmentIndex = index;
                    segment.segmentCount = count;

                    std::lock_guard<std::mutex> lock(m_mutex);

                    PushEventLocked(std::move(segment));
                }
            );

            // 被跳过 / 退出程序打断，不算错误。
            if (!talk.ok && !talk.interrupted && !m_stopping)
            {
                ReportError("TTS 播放", talk.error);

                note = "语音没有完整播放";
            }
        }
        else
        {
            note = "没有语音";
        }

        // 没有语音，或者语音一个都没播出来：直接开始动作 / 表情。
        // 被跳过的话就不开始了。
        submitActions();

        // 等动作 / 表情表达完成（开始后 10 秒）。被跳过时立刻结束等待。
        if (result.hasActions && !m_skip.load() && !m_stopping)
        {
            m_play.WaitFinished(m_skip);
        }

        const bool skipped = m_skip.load() && !m_stopping.load();

        bool nothingQueued = false;

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_currentSeq = 0;

            nothingQueued = m_prepared.empty();
        }

        // 周期正常结束且后面没有排队的任务：回到待机状态（可选）。
        if (!skipped && !m_stopping && nothingQueued
            && m_config.returnToRestAfterCycle)
        {
            m_play.RequestStop(true);
        }

        {
            AIEvent finished;

            finished.type = AIEvent::Type::SpeechFinished;
            finished.taskId = seq;
            finished.text = replyText;
            finished.skipped = skipped;

            std::lock_guard<std::mutex> lock(m_mutex);

            PushEventLocked(std::move(finished));
        }

        FinishCycle(seq, skipped ? TaskState::Skipped : TaskState::Done,
            skipped ? "已跳过" : note);
    }


    // ============================================================
    // 后台善后
    // ============================================================

    void AIManager::CleanupLoop()
    {
        while (!m_stopping)
        {
            CleanupItem item;

            {
                std::unique_lock<std::mutex> lock(m_mutex);

                m_condition.wait(lock, [this]
                {
                    return m_stopping.load() || !m_cleanup.empty();
                });

                if (m_stopping)
                {
                    return;
                }

                item = std::move(m_cleanup.front());

                m_cleanup.pop_front();
            }

            // 先撤对话历史（很快），再等语音生成完删文件（可能要几秒）。
            if (!item.forgetUserText.empty() && m_chatApi != nullptr)
            {
                m_chatApi->ForgetExchange(item.forgetUserText);
            }

            if (!item.ttsJson.empty())
            {
                m_talk.Discard(item.ttsJson, m_stopping);
            }
        }
    }
}
