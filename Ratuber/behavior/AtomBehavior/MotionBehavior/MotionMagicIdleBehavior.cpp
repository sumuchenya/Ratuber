#include "MotionMagicIdleBehavior.h"

#include "../../../motion/MotionManager.h"

namespace Ratuber
{

    MotionMagicIdleBehavior::MotionMagicIdleBehavior()
        :
        m_motionManager(nullptr)
    {}

    MotionMagicIdleBehavior::~MotionMagicIdleBehavior()
    {}

    void MotionMagicIdleBehavior::Initialize(
        MotionManager* motionManager
    )
    {
        m_motionManager = motionManager;

        if (!m_motionManager)
        {
            return;
        }

        m_motionManager->PlayMotion(
            "sample_01",
            2
        );
    }

    void MotionMagicIdleBehavior::Update(
        float deltaTime
    )
    {
        // 当前基础动作行为不需要持续更新
    }

}