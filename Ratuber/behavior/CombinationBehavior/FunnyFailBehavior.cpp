#include "FunnyFailBehavior.h"

namespace Ratuber
{

FunnyFailBehavior::FunnyFailBehavior()
    : m_initialized(false)
{}

FunnyFailBehavior::~FunnyFailBehavior()
{}

void FunnyFailBehavior::Initialize(
    MotionManager* motionManager,
    ExpressionManager* expressionManager
)
{
    m_motionBehavior.Initialize(motionManager);
    m_expressionBehavior.Initialize(expressionManager);

    m_initialized = true;
}

void FunnyFailBehavior::Update(float deltaTime)
{
    if (!m_initialized)
        return;

    m_motionBehavior.Update(deltaTime);
    m_expressionBehavior.Update(deltaTime);
}

}
