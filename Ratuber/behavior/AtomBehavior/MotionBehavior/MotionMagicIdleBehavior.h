#pragma once

namespace Ratuber
{

    class MotionManager;

    class MotionMagicIdleBehavior
    {
    public:
        MotionMagicIdleBehavior();
        ~MotionMagicIdleBehavior();

        void Initialize(MotionManager* motionManager);
        void Update(float deltaTime);

    private:
        MotionManager* m_motionManager;
    };

}