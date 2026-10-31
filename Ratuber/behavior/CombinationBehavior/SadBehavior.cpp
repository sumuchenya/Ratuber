#include "SadBehavior.h"

namespace Ratuber
{

SadBehavior::SadBehavior()
    : m_initialized(false)
{}

SadBehavior::~SadBehavior()
{}

void SadBehavior::Initialize(ExpressionManager* expressionManager)
{
    m_expressionBehavior.Initialize(expressionManager);

    m_initialized = true;
}

void SadBehavior::Update(float deltaTime)
{
    if (!m_initialized)
        return;

    m_expressionBehavior.Update(deltaTime);
}

}
