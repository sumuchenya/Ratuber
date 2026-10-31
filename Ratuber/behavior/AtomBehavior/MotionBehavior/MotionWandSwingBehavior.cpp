#include "MotionWandSwingBehavior.h"

#include "../../../motion/MotionManager.h"

namespace Ratuber
{

    MotionWandSwingBehavior::MotionWandSwingBehavior()
        :
        m_motionManager(nullptr)
    {}

    MotionWandSwingBehavior::~MotionWandSwingBehavior()
    {}

    void MotionWandSwingBehavior::Initialize(
        MotionManager* motionManager
    )
    {
        m_motionManager = motionManager;

        if (!m_motionManager)
        {
            return;
        }

        m_motionManager->PlayMotion(
            "mtn_03",
            2
        );
    }

    void MotionWandSwingBehavior::Update(
        float deltaTime
    )
    {
        // 当前基础动作行为不需要持续更新
    }

}