#include "AngryBehavior.h"

namespace Ratuber
{

AngryBehavior::AngryBehavior()
    : m_initialized(false)
{}

AngryBehavior::~AngryBehavior()
{}

void AngryBehavior::Initialize(
    MotionManager* motionManager,
    ExpressionManager* expressionManager
)
{
    m_motionBehavior.Initialize(motionManager);
    m_expressionBehavior.Initialize(expressionManager);

    m_initialized = true;
}

void AngryBehavior::Update(float deltaTime)
{
    if (!m_initialized)
        return;

    m_motionBehavior.Update(deltaTime);
    m_expressionBehavior.Update(deltaTime);
}

}
