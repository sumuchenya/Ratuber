#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>

// ============================================================
// NowICanPlay.h
//
// 职责：把 ai_server.py 决定好的动作 / 表情表达出来。
//
// 输入是 ai_server.py 的输出（POST /decide 的响应），例如：
//
//   {
//     "success": true,
//     "emotion": "happy",
//     "level": "compose",
//     "reason": "compose_success",
//     "actions": {
//       "motion": "motion_wand_swing",
//       "expression": "expression_smile"
//     }
//   }
//
// 只截取其中的 "actions"，然后调用 behavior 目录里现成的行为类：
//
//   motion_wand_swing   -> MotionWandSwingBehavior
//   expression_smile    -> ExpressionSmileBehavior
//
// 动作 / 表情一开始表达，就计时 10 秒，10 秒后视为表达完成。
//
// 线程：
//   Submit()        任意线程可调用（只是登记，不碰模型）。
//   Update()        必须在主线程每帧调用（真正触发动作、表情）。
//   WaitFinished()  在工作线程里等待“表达完成”。
// ============================================================

namespace Ratuber
{
    class MotionManager;
    class ExpressionManager;
}

namespace Ratuber::AI
{
    // 动作 / 表情开始表达之后，过多久算表达完成。
    inline constexpr double kActionExpressSeconds = 10.0;

    // 包装一个行为类。具体定义在 .cpp 里。
    struct IBehaviorRunner;

    class NowICanPlay
    {
    public:
        NowICanPlay();

        ~NowICanPlay();

        void Initialize(
            MotionManager* motionManager,
            ExpressionManager* expressionManager
        );

        // 登记一次要表达的动作 / 表情。
        // 返回 false 表示响应解析失败。
        // actions 里什么都没有时视为立即完成。
        bool Submit(const std::string& decideResponseJson);

        // 主线程每帧调用。
        void Update(float deltaTime);

        // 这次登记的动作 / 表情是否已经表达完成。
        bool IsFinished() const
        {
            return m_finished.load();
        }

        // 等到表达完成。
        // stop 变为 true 时提前返回。
        // 主线程迟迟没有调用 Update（超过 pickupTimeoutMs）时放弃并返回 false。
        bool WaitFinished(
            const std::atomic<bool>& stop,
            int pickupTimeoutMs = 5000
        );

        // 立刻视为完成（程序退出时用）。
        void Abort();

        // 停下当前的动作和表情（用户点了“跳过”）。任意线程可调用，
        // 真正的停止在主线程的下一次 Update() 里执行。
        //
        // returnToRest 为 true 时，停下之后再启动待机动作（MotionIdle）
        // 和中性表情（ExpressionNeutral），让模型回到站立状态。
        // 如果同一时刻又有新的 Submit()，新的会直接接替，不会先回待机。
        void RequestStop(bool returnToRest);

    private:
        MotionManager* m_motionManager = nullptr;

        ExpressionManager* m_expressionManager = nullptr;

        std::mutex m_mutex;

        bool m_hasPending = false;

        bool m_stopRequested = false;

        bool m_stopReturnToRest = false;

        std::string m_pendingMotion;

        std::string m_pendingExpression;

        std::unique_ptr<IBehaviorRunner> m_motionRunner;

        std::unique_ptr<IBehaviorRunner> m_expressionRunner;

        std::chrono::steady_clock::time_point m_startTime;

        // 已经开始表达、还没满 10 秒。
        std::atomic<bool> m_running{ false };

        std::atomic<bool> m_finished{ true };
    };
}
