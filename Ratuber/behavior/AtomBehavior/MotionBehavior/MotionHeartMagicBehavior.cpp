#include "MotionHeartMagicBehavior.h"

#include "../../../motion/MotionManager.h"

namespace Ratuber
{

    MotionHeartMagicBehavior::MotionHeartMagicBehavior()
        :
        m_motionManager(nullptr)
    {}

    MotionHeartMagicBehavior::~MotionHeartMagicBehavior()
    {}

    void MotionHeartMagicBehavior::Initialize(
        MotionManager* motionManager
    )
    {
        m_motionManager = motionManager;

        if (!m_motionManager)
        {
            return;
        }

        m_motionManager->PlayMotion(
            "special_01",
            2
        );
    }

    void MotionHeartMagicBehavior::Update(
        float deltaTime
    )
    {
        // 当前基础动作行为不需要持续更新
    }

}