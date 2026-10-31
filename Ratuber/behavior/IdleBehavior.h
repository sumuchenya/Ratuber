#pragma once

#include <memory>

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
    class FunnyFailBehavior;

    // 测试用：同时运行 晃动 + 眨眼 + 张嘴 + FunnyFailBehavior。
    class IdleBehavior
    {
    public:
        IdleBehavior();
        ~IdleBehavior();

        void SetModel(
            Live2D::Cubism::Framework::CubismModel* model
        );

        void Initialize(
            MotionManager* motionManager,
            ExpressionManager* expressionManager,
            BlinkManager* blinkManager,
            MouthOpenManager* mouthOpenManager
        );

        void Update(float deltaTime);

    private:
        MotionManager* m_motionManager;
        BlinkManager* m_blinkManager;
        MouthOpenManager* m_mouthOpenManager;
        Live2D::Cubism::Framework::CubismModel* m_model;

        std::unique_ptr<Blink> m_blink;
        std::unique_ptr<Mouth> m_mouth;
        std::unique_ptr<Shake> m_shake;
        std::unique_ptr<FunnyFailBehavior> m_funnyFailBehavior;

        bool m_shakeReady;
        bool m_initialized;
    };
}