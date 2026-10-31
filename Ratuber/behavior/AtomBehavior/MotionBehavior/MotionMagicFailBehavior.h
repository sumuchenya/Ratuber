#pragma once

namespace Ratuber
{

    class MotionManager;

    class MotionMagicFailBehavior
    {
    public:
        MotionMagicFailBehavior();
        ~MotionMagicFailBehavior();

        void Initialize(MotionManager* motionManager);
        void Update(float deltaTime);

    private:
        MotionManager* m_motionManager;
    };

}