#pragma once

#include "../AtomBehavior/MotionBehavior/MotionHatBehavior.h"
#include "../AtomBehavior/ExpressionBehavior/ExpressionSurprisedBehavior.h"

namespace Ratuber
{

class MotionManager;
class ExpressionManager;

class SurprisedBehavior
{
public:
    SurprisedBehavior();
    ~SurprisedBehavior();

    void Initialize(
        MotionManager* motionManager,
        ExpressionManager* expressionManager
    );

    void Update(float deltaTime);

private:
    MotionHatBehavior m_motionBehavior;
    ExpressionSurprisedBehavior m_expressionBehavior;

    bool m_initialized;
};

}
