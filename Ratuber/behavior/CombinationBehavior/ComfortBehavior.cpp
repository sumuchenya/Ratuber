#include "ComfortBehavior.h"

namespace Ratuber
{

ComfortBehavior::ComfortBehavior()
    : m_initialized(false)
{}

ComfortBehavior::~ComfortBehavior()
{}

void ComfortBehavior::Initialize(
    MotionManager* motionManager,
    ExpressionManager* expressionManager
)
{
    m_motionBehavior.Initialize(motionManager);
    m_expressionBehavior.Initialize(expressionManager);

    m_initialized = true;
}

void ComfortBehavior::Update(float deltaTime)
{
    if (!m_initialized)
        return;

    m_motionBehavior.Update(deltaTime);
    m_expressionBehavior.Update(deltaTime);
}

}
