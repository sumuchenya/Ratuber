#pragma once

#include "../AtomBehavior/MotionBehavior/MotionHeartMagicBehavior.h"
#include "../AtomBehavior/ExpressionBehavior/ExpressionSmileBehavior.h"

namespace Ratuber
{

class MotionManager;
class ExpressionManager;

class ThankYouBehavior
{
public:
    ThankYouBehavior();
    ~ThankYouBehavior();

    void Initialize(
        MotionManager* motionManager,
        ExpressionManager* expressionManager
    );

    void Update(float deltaTime);

private:
    MotionHeartMagicBehavior m_motionBehavior;
    ExpressionSmileBehavior m_expressionBehavior;

    bool m_initialized;
};

}
