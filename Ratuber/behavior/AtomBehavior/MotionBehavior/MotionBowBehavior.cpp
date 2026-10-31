#include "MotionBowBehavior.h"

#include "../../../motion/MotionManager.h"

namespace Ratuber
{

    MotionBowBehavior::MotionBowBehavior()
        :
        m_motionManager(nullptr)
    {}

    MotionBowBehavior::~MotionBowBehavior()
    {}

    void MotionBowBehavior::Initialize(
        MotionManager* motionManager
    )
    {
        m_motionManager = motionManager;

        if (!m_motionManager)
        {
            return;
        }

        m_motionManager->PlayMotion(
            "mtn_02",
            2
        );
    }

    void MotionBowBehavior::Update(
        float deltaTime
    )
    {
        // 当前基础动作行为不需要持续更新
    }

}