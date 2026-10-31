#include "HappyBehavior.h"

namespace Ratuber
{

HappyBehavior::HappyBehavior()
    : m_initialized(false)
{}

HappyBehavior::~HappyBehavior()
{}

void HappyBehavior::Initialize(
    MotionManager* motionManager,
    ExpressionManager* expressionManager
)
{
    m_motionBehavior.Initialize(motionManager);
    m_expressionBehavior.Initialize(expressionManager);

    m_initialized = true;
}

void HappyBehavior::Update(float deltaTime)
{
    if (!m_initialized)
        return;

    m_motionBehavior.Update(deltaTime);
    m_expressionBehavior.Update(deltaTime);
}

}
