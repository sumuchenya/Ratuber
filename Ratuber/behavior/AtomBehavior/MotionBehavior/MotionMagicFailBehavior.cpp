#include "MotionMagicFailBehavior.h"

#include "../../../motion/MotionManager.h"

namespace Ratuber
{

    MotionMagicFailBehavior::MotionMagicFailBehavior()
        :
        m_motionManager(nullptr)
    {}

    MotionMagicFailBehavior::~MotionMagicFailBehavior()
    {}

    void MotionMagicFailBehavior::Initialize(
        MotionManager* motionManager
    )
    {
        m_motionManager = motionManager;

        if (!m_motionManager)
        {
            return;
        }

        m_motionManager->PlayMotion(
            "special_02",
            2
        );
    }

    void MotionMagicFailBehavior::Update(
        float deltaTime
    )
    {
        // 当前基础动作行为不需要持续更新
    }

}