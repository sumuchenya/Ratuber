#pragma once

#include "../AtomBehavior/MotionBehavior/MotionBowBehavior.h"
#include "../AtomBehavior/ExpressionBehavior/ExpressionSmileBehavior.h"

namespace Ratuber
{

class MotionManager;
class ExpressionManager;

class GreetingBehavior
{
public:
    GreetingBehavior();
    ~GreetingBehavior();

    void Initialize(
        MotionManager* motionManager,
        ExpressionManager* expressionManager
    );

    void Update(float deltaTime);

private:
    MotionBowBehavior m_motionBehavior;
    ExpressionSmileBehavior m_expressionBehavior;

    bool m_initialized;
};

}
