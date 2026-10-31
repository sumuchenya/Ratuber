#pragma once

namespace Ratuber
{

    class MotionManager;

    class MotionWandSwingBehavior
    {
    public:
        MotionWandSwingBehavior();
        ~MotionWandSwingBehavior();

        void Initialize(MotionManager* motionManager);
        void Update(float deltaTime);

    private:
        MotionManager* m_motionManager;
    };

}