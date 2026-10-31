#include "ThankYouBehavior.h"

namespace Ratuber
{

ThankYouBehavior::ThankYouBehavior()
    : m_initialized(false)
{}

ThankYouBehavior::~ThankYouBehavior()
{}

void ThankYouBehavior::Initialize(
    MotionManager* motionManager,
    ExpressionManager* expressionManager
)
{
    m_motionBehavior.Initialize(motionManager);
    m_expressionBehavior.Initialize(expressionManager);

    m_initialized = true;
}

void ThankYouBehavior::Update(float deltaTime)
{
    if (!m_initialized)
        return;

    m_motionBehavior.Update(deltaTime);
    m_expressionBehavior.Update(deltaTime);
}

}
