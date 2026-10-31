#include "MotionRabbitMagicBehavior.h"

#include "../../../motion/MotionManager.h"

namespace Ratuber
{

    MotionRabbitMagicBehavior::MotionRabbitMagicBehavior()
        :
        m_motionManager(nullptr)
    {}

    MotionRabbitMagicBehavior::~MotionRabbitMagicBehavior()
    {}

    void MotionRabbitMagicBehavior::Initialize(
        MotionManager* motionManager
    )
    {
        m_motionManager = motionManager;

        if (!m_motionManager)
        {
            return;
        }

        m_motionManager->PlayMotion(
            "special_03",
            2
        );
    }

    void MotionRabbitMagicBehavior::Update(
        float deltaTime
    )
    {
        // 当前基础动作行为不需要持续更新
    }

}