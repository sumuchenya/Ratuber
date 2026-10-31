#pragma once

namespace Ratuber
{

    class MotionManager;

    class MotionHeartMagicBehavior
    {
    public:
        MotionHeartMagicBehavior();
        ~MotionHeartMagicBehavior();

        void Initialize(MotionManager* motionManager);
        void Update(float deltaTime);

    private:
        MotionManager* m_motionManager;
    };

}