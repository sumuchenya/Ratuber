#pragma once

#include "../AtomBehavior/MotionBehavior/MotionHatBehavior.h"
#include "../AtomBehavior/ExpressionBehavior/ExpressionAngryBehavior.h"

namespace Ratuber
{

class MotionManager;
class ExpressionManager;

class AngryBehavior
{
public:
    AngryBehavior();
    ~AngryBehavior();

    void Initialize(
        MotionManager* motionManager,
        ExpressionManager* expressionManager
    );

    void Update(float deltaTime);

private:
    MotionHatBehavior m_motionBehavior;
    ExpressionAngryBehavior m_expressionBehavior;

    bool m_initialized;
};

}
