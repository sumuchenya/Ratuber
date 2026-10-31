#include "GreetingBehavior.h"

namespace Ratuber
{

GreetingBehavior::GreetingBehavior()
    : m_initialized(false)
{}

GreetingBehavior::~GreetingBehavior()
{}

void GreetingBehavior::Initialize(
    MotionManager* motionManager,
    ExpressionManager* expressionManager
)
{
    m_motionBehavior.Initialize(motionManager);
    m_expressionBehavior.Initialize(expressionManager);

    m_initialized = true;
}

void GreetingBehavior::Update(float deltaTime)
{
    if (!m_initialized)
        return;

    m_motionBehavior.Update(deltaTime);
    m_expressionBehavior.Update(deltaTime);
}

}
