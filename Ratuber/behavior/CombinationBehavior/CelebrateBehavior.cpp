#include "CelebrateBehavior.h"

namespace Ratuber
{

CelebrateBehavior::CelebrateBehavior()
    : m_initialized(false)
{}

CelebrateBehavior::~CelebrateBehavior()
{}

void CelebrateBehavior::Initialize(
    MotionManager* motionManager,
    ExpressionManager* expressionManager
)
{
    m_motionBehavior.Initialize(motionManager);
    m_expressionBehavior.Initialize(expressionManager);

    m_initialized = true;
}

void CelebrateBehavior::Update(float deltaTime)
{
    if (!m_initialized)
        return;

    m_motionBehavior.Update(deltaTime);
    m_expressionBehavior.Update(deltaTime);
}

}
