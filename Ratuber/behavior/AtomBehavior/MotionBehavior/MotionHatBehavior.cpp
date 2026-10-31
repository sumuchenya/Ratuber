#include "MotionHatBehavior.h"

#include "../../../motion/MotionManager.h"

namespace Ratuber
{

    MotionHatBehavior::MotionHatBehavior()
        :
        m_motionManager(nullptr)
    {}

    MotionHatBehavior::~MotionHatBehavior()
    {}

    void MotionHatBehavior::Initialize(
        MotionManager* motionManager
    )
    {
        m_motionManager = motionManager;

        if (!m_motionManager)
        {
            return;
        }

        m_motionManager->PlayMotion(
            "mtn_04",
            2
        );
    }

    void MotionHatBehavior::Update(
        float deltaTime
    )
    {
        // 当前基础动作行为不需要持续更新
    }

}