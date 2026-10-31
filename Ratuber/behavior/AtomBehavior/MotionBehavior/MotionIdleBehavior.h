#pragma once

namespace Ratuber
{

    class MotionManager;

    class MotionIdleBehavior
    {
    public:
        MotionIdleBehavior();
        ~MotionIdleBehavior();

        void Initialize(MotionManager* motionManager);
        void Update(float deltaTime);

    private:
        MotionManager* m_motionManager;
    };

}