#include "NowICanPlay.h"

#include <cctype>
#include <thread>

#include "../AIPlatform.h"
#include "../AIProtocol.h"

#include "expression/ExpressionManager.h"
#include "motion/MotionManager.h"

// ---- 原子行为：8 个动作 ----
#include "behavior/AtomBehavior/MotionBehavior/MotionBowBehavior.h"
#include "behavior/AtomBehavior/MotionBehavior/MotionHatBehavior.h"
#include "behavior/AtomBehavior/MotionBehavior/MotionHeartMagicBehavior.h"
#include "behavior/AtomBehavior/MotionBehavior/MotionIdleBehavior.h"
#include "behavior/AtomBehavior/MotionBehavior/MotionMagicFailBehavior.h"
#include "behavior/AtomBehavior/MotionBehavior/MotionMagicIdleBehavior.h"
#include "behavior/AtomBehavior/MotionBehavior/MotionRabbitMagicBehavior.h"
#include "behavior/AtomBehavior/MotionBehavior/MotionWandSwingBehavior.h"

// ---- 原子行为：8 个表情 ----
#include "behavior/AtomBehavior/ExpressionBehavior/ExpressionAngryBehavior.h"
#include "behavior/AtomBehavior/ExpressionBehavior/ExpressionCloseEyesBehavior.h"
#include "behavior/AtomBehavior/ExpressionBehavior/ExpressionEmbarrassedBehavior.h"
#include "behavior/AtomBehavior/ExpressionBehavior/ExpressionNeutralBehavior.h"
#include "behavior/AtomBehavior/ExpressionBehavior/ExpressionSadBehavior.h"
#include "behavior/AtomBehavior/ExpressionBehavior/ExpressionShyBehavior.h"
#include "behavior/AtomBehavior/ExpressionBehavior/ExpressionSmileBehavior.h"
#include "behavior/AtomBehavior/ExpressionBehavior/ExpressionSurprisedBehavior.h"

namespace Ratuber::AI
{
    namespace
    {
        constexpr const char* kTag = "NowICanPlay";

        template <class>
        inline constexpr bool kAlwaysFalse = false;


        // ============================================================
        // 行为类的调用适配
        //
        // 各个行为类的 Initialize 形参略有不同（有的只要 MotionManager，
        // 有的只要 ExpressionManager，组合行为两个都要），
        // 这里在编译期自动选出能用的那一种。
        // 全都不匹配时，会在编译时给出清楚的报错，改这一处即可。
        // ============================================================

        template <class T>
        void InitializeBehavior(
            T& behavior,
            MotionManager* motion,
            ExpressionManager* expression
        )
        {
            if constexpr (requires { behavior.Initialize(motion, expression); })
            {
                behavior.Initialize(motion, expression);
            }
            else if constexpr (requires { behavior.Initialize(motion); })
            {
                behavior.Initialize(motion);
            }
            else if constexpr (requires { behavior.Initialize(expression); })
            {
                behavior.Initialize(expression);
            }
            else if constexpr (requires { behavior.Initialize(expression, motion); })
            {
                behavior.Initialize(expression, motion);
            }
            else
            {
                static_assert(
                    kAlwaysFalse<T>,
                    "NowICanPlay：不认识这个行为类的 Initialize 形参，"
                    "请在 InitializeBehavior 里补一种调用方式"
                );
            }
        }

        template <class T>
        void UpdateBehavior(T& behavior, float deltaTime)
        {
            if constexpr (requires { behavior.Update(deltaTime); })
            {
                behavior.Update(deltaTime);
            }
        }
    }


    // ============================================================
    // 行为包装
    // ============================================================

    struct IBehaviorRunner
    {
        virtual ~IBehaviorRunner() = default;

        virtual void Update(float deltaTime) = 0;
    };

    namespace
    {
        template <class T>
        class BehaviorRunner final : public IBehaviorRunner
        {
        public:
            BehaviorRunner(
                MotionManager* motion,
                ExpressionManager* expression
            )
            {
                InitializeBehavior(m_behavior, motion, expression);
            }

            void Update(float deltaTime) override
            {
                UpdateBehavior(m_behavior, deltaTime);
            }

        private:
            T m_behavior;
        };

        using Factory =
            std::unique_ptr<IBehaviorRunner>(*)(
                MotionManager*,
                ExpressionManager*
                );

        struct Entry
        {
            // 类名去掉 Behavior 后缀，全小写，例如 motionwandswing。
            const char* key;

            Factory create;
        };

        template <class T>
        std::unique_ptr<IBehaviorRunner> Create(
            MotionManager* motion,
            ExpressionManager* expression
        )
        {
            return std::make_unique<BehaviorRunner<T>>(
                motion,
                expression
            );
        }

        const Entry kMotionTable[] =
        {
            { "motionbow",         &Create<MotionBowBehavior> },
            { "motionhat",         &Create<MotionHatBehavior> },
            { "motionheartmagic",  &Create<MotionHeartMagicBehavior> },
            { "motionidle",        &Create<MotionIdleBehavior> },
            { "motionmagicfail",   &Create<MotionMagicFailBehavior> },
            { "motionmagicidle",   &Create<MotionMagicIdleBehavior> },
            { "motionrabbitmagic", &Create<MotionRabbitMagicBehavior> },
            { "motionwandswing",   &Create<MotionWandSwingBehavior> },
        };

        const Entry kExpressionTable[] =
        {
            { "expressionangry",       &Create<ExpressionAngryBehavior> },
            { "expressioncloseeyes",   &Create<ExpressionCloseEyesBehavior> },
            { "expressionembarrassed", &Create<ExpressionEmbarrassedBehavior> },
            { "expressionneutral",     &Create<ExpressionNeutralBehavior> },
            { "expressionsad",         &Create<ExpressionSadBehavior> },
            { "expressionshy",         &Create<ExpressionShyBehavior> },
            { "expressionsmile",       &Create<ExpressionSmileBehavior> },
            { "expressionsurprised",   &Create<ExpressionSurprisedBehavior> },
        };

        // 统一写法：去掉下划线、横线、空格，转小写，去掉 behavior 后缀。
        // "motion_wand_swing" / "MotionWandSwingBehavior" 都变成 "motionwandswing"。
        std::string Normalize(const std::string& id)
        {
            std::string key;

            for (const unsigned char c : id)
            {
                if (std::isalnum(c))
                {
                    key.push_back(static_cast<char>(std::tolower(c)));
                }
            }

            const std::string suffix = "behavior";

            if (key.size() > suffix.size()
                && key.compare(
                    key.size() - suffix.size(),
                    suffix.size(),
                    suffix) == 0)
            {
                key.resize(key.size() - suffix.size());
            }

            return key;
        }

        template <size_t N>
        std::unique_ptr<IBehaviorRunner> Build(
            const Entry(&table)[N],
            const std::string& id,
            MotionManager* motion,
            ExpressionManager* expression
        )
        {
            const std::string key = Normalize(id);

            for (const Entry& entry : table)
            {
                if (key == entry.key)
                {
                    return entry.create(motion, expression);
                }
            }

            return nullptr;
        }
    }


    // ============================================================
    // NowICanPlay
    // ============================================================

    NowICanPlay::NowICanPlay() = default;

    NowICanPlay::~NowICanPlay() = default;

    void NowICanPlay::Initialize(
        MotionManager* motionManager,
        ExpressionManager* expressionManager
    )
    {
        m_motionManager = motionManager;
        m_expressionManager = expressionManager;
    }

    bool NowICanPlay::Submit(const std::string& decideResponseJson)
    {
        Protocol::Actions actions;
        std::string level;
        std::string error;

        if (!Protocol::ParseDecideResponse(
                decideResponseJson, actions, level, error))
        {
            Platform::Log(kTag, "解析 ai_server 的输出失败：" + error);

            m_finished = true;

            return false;
        }

        // 什么都不用表达，就没有“表达完成”要等。
        if (actions.Empty())
        {
            m_finished = true;

            return true;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            // 必须先标记“未完成”再登记，
            // 否则主线程可能先取走并立刻判定完成，随后又被这里改回 false。
            m_finished = false;

            m_pendingMotion = actions.motion;
            m_pendingExpression = actions.expression;
            m_hasPending = true;
        }

        return true;
    }

    void NowICanPlay::Update(float deltaTime)
    {
        bool start = false;
        bool stop = false;
        bool returnToRest = false;

        std::string motionId;
        std::string expressionId;

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_stopRequested)
            {
                stop = true;
                returnToRest = m_stopReturnToRest;

                m_stopRequested = false;
                m_stopReturnToRest = false;
            }

            if (m_hasPending)
            {
                start = true;

                motionId = std::move(m_pendingMotion);
                expressionId = std::move(m_pendingExpression);

                m_pendingMotion.clear();
                m_pendingExpression.clear();

                m_hasPending = false;
            }
        }

        // ---- 跳过：停下当前的动作和表情 ----
        //
        // 不改 m_finished：RequestStop 已经把它置为 true，
        // 如果之后又有新的 Submit，它被置回 false，这里不能再覆盖。

        if (stop)
        {
            m_motionRunner.reset();
            m_expressionRunner.reset();

            m_running = false;

            if (m_motionManager != nullptr)
            {
                m_motionManager->StopMotion();
            }

            if (m_expressionManager != nullptr)
            {
                m_expressionManager->StopExpression();
            }

            // 新的一次紧跟着来，就不用先回待机了。
            if (returnToRest && !start
                && m_motionManager != nullptr
                && m_expressionManager != nullptr)
            {
                m_motionRunner = Build(
                    kMotionTable, "motion_idle",
                    m_motionManager, m_expressionManager);

                m_expressionRunner = Build(
                    kExpressionTable, "expression_neutral",
                    m_motionManager, m_expressionManager);

                Platform::Log(kTag, "已停止，回到待机状态");
            }
        }

        // ---- 新的一次：换上新的行为 ----

        if (start)
        {
            m_motionRunner.reset();
            m_expressionRunner.reset();

            if (m_motionManager == nullptr
                || m_expressionManager == nullptr)
            {
                Platform::Log(kTag,
                    "还没有 Initialize（MotionManager / ExpressionManager 为空）");

                m_running = false;
                m_finished = true;

                return;
            }

            if (!motionId.empty())
            {
                m_motionRunner = Build(
                    kMotionTable,
                    motionId,
                    m_motionManager,
                    m_expressionManager
                );

                if (!m_motionRunner)
                {
                    Platform::Log(kTag, "不认识的动作 id：" + motionId
                        + "（对照 behaviors.json 和 NowICanPlay.cpp 里的表）");
                }
            }

            if (!expressionId.empty())
            {
                m_expressionRunner = Build(
                    kExpressionTable,
                    expressionId,
                    m_motionManager,
                    m_expressionManager
                );

                if (!m_expressionRunner)
                {
                    Platform::Log(kTag, "不认识的表情 id：" + expressionId
                        + "（对照 behaviors.json 和 NowICanPlay.cpp 里的表）");
                }
            }

            if (!m_motionRunner && !m_expressionRunner)
            {
                m_running = false;
                m_finished = true;

                return;
            }

            Platform::Log(kTag, "开始表达：motion="
                + (motionId.empty() ? std::string("无") : motionId)
                + " expression="
                + (expressionId.empty() ? std::string("无") : expressionId));

            m_startTime = std::chrono::steady_clock::now();
            m_running = true;
        }

        // ---- 每帧更新 ----
        //
        // 满 10 秒之后也继续更新，直到下一次登记换掉它们。
        // 这和 IdleBehavior 里“每帧 Update 一直调用”的用法一致。

        if (m_motionRunner)
        {
            m_motionRunner->Update(deltaTime);
        }

        if (m_expressionRunner)
        {
            m_expressionRunner->Update(deltaTime);
        }

        if (m_running)
        {
            const std::chrono::duration<double> elapsed =
                std::chrono::steady_clock::now() - m_startTime;

            if (elapsed.count() >= kActionExpressSeconds)
            {
                m_running = false;
                m_finished = true;
            }
        }
    }

    bool NowICanPlay::WaitFinished(
        const std::atomic<bool>& stop,
        int pickupTimeoutMs
    )
    {
        const auto begin = std::chrono::steady_clock::now();

        while (!stop.load())
        {
            if (m_finished.load())
            {
                return true;
            }

            // 还没有被主线程取走。
            if (!m_running.load())
            {
                const auto waited =
                    std::chrono::steady_clock::now() - begin;

                if (waited > std::chrono::milliseconds(pickupTimeoutMs))
                {
                    Platform::Log(kTag,
                        "等不到主线程开始表达，放弃等待"
                        "（主循环有没有每帧调用 AIManager::Update？）");

                    {
                        std::lock_guard<std::mutex> lock(m_mutex);

                        m_hasPending = false;
                    }

                    m_finished = true;

                    return false;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        return false;
    }

    void NowICanPlay::RequestStop(bool returnToRest)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_hasPending = false;

            m_stopRequested = true;
            m_stopReturnToRest = returnToRest;
        }

        m_running = false;
        m_finished = true;
    }

    void NowICanPlay::Abort()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_hasPending = false;
        }

        m_running = false;
        m_finished = true;
    }
}
