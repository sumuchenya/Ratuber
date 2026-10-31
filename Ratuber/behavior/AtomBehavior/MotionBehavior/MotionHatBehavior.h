#pragma once

namespace Ratuber
{

    class MotionManager;

    class MotionHatBehavior
    {
    public:
        MotionHatBehavior();
        ~MotionHatBehavior();

        void Initialize(MotionManager* motionManager);
        void Update(float deltaTime);

    private:
        MotionManager* m_motionManager;
    };

}