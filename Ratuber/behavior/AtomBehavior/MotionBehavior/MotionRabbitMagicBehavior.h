#pragma once

namespace Ratuber
{

    class MotionManager;

    class MotionRabbitMagicBehavior
    {
    public:
        MotionRabbitMagicBehavior();
        ~MotionRabbitMagicBehavior();

        void Initialize(MotionManager* motionManager);
        void Update(float deltaTime);

    private:
        MotionManager* m_motionManager;
    };

}