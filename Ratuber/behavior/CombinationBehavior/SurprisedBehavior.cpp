#include "SurprisedBehavior.h"

namespace Ratuber
{

SurprisedBehavior::SurprisedBehavior()
    : m_initialized(false)
{}

SurprisedBehavior::~SurprisedBehavior()
{}

void SurprisedBehavior::Initialize(
    MotionManager* motionManager,
    ExpressionManager* expressionManager
)
{
    m_motionBehavior.Initialize(motionManager);
    m_expressionBehavior.Initialize(expressionManager);

    m_initialized = true;
}

void SurprisedBehavior::Update(float deltaTime)
{
    if (!m_initialized)
        return;

    m_motionBehavior.Update(deltaTime);
    m_expressionBehavior.Update(deltaTime);
}

}
