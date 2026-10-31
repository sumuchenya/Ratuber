#pragma once

#include "../AtomBehavior/MotionBehavior/MotionRabbitMagicBehavior.h"
#include "../AtomBehavior/ExpressionBehavior/ExpressionSmileBehavior.h"

namespace Ratuber
{

class MotionManager;
class ExpressionManager;

class CelebrateBehavior
{
public:
    CelebrateBehavior();
    ~CelebrateBehavior();

    void Initialize(
        MotionManager* motionManager,
        ExpressionManager* expressionManager
    );

    void Update(float deltaTime);

private:
    MotionRabbitMagicBehavior m_motionBehavior;
    ExpressionSmileBehavior m_expressionBehavior;

    bool m_initialized;
};

}
