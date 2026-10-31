#include "ShyBehavior.h"

namespace Ratuber
{

ShyBehavior::ShyBehavior()
    : m_initialized(false)
{}

ShyBehavior::~ShyBehavior()
{}

void ShyBehavior::Initialize(
    MotionManager* motionManager,
    ExpressionManager* expressionManager
)
{
    m_motionBehavior.Initialize(motionManager);

    m_smileBehavior.Initialize(expressionManager);
    m_shyBehavior.Initialize(expressionManager);

    m_initialized = true;
}

void ShyBehavior::Update(float deltaTime)
{
    if (!m_initialized)
        return;

    m_motionBehavior.Update(deltaTime);

    m_smileBehavior.Update(deltaTime);
    m_shyBehavior.Update(deltaTime);
}

}
