#pragma once

namespace Ratuber
{

    class MotionManager;

    class MotionBowBehavior
    {
    public:
        MotionBowBehavior();
        ~MotionBowBehavior();

        void Initialize(MotionManager* motionManager);
        void Update(float deltaTime);

    private:
        MotionManager* m_motionManager;
    };

}