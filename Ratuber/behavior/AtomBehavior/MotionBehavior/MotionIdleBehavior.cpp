#include "MotionIdleBehavior.h"

#include "../../../motion/MotionManager.h"

namespace Ratuber
{

    MotionIdleBehavior::MotionIdleBehavior()
        :
        m_motionManager(nullptr)
    {}

    MotionIdleBehavior::~MotionIdleBehavior()
    {}

    void MotionIdleBehavior::Initialize(
        MotionManager* motionManager
    )
    {
        m_motionManager = motionManager;

        if (!m_motionManager)
        {
            return;
        }

        m_motionManager->PlayMotion(
            "mtn_01",
            2
        );
    }

    void MotionIdleBehavior::Update(
        float deltaTime
    )
    {
        // 当前基础动作行为不需要持续更新
    }

}