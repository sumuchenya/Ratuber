#pragma once

#include "../AtomBehavior/MotionBehavior/MotionWandSwingBehavior.h"
#include "../AtomBehavior/ExpressionBehavior/ExpressionSmileBehavior.h"

namespace Ratuber
{

class MotionManager;
class ExpressionManager;

class HappyBehavior
{
public:
    HappyBehavior();
    ~HappyBehavior();

    void Initialize(
        MotionManager* motionManager,
        ExpressionManager* expressionManager
    );

    void Update(float deltaTime);

private:
    MotionWandSwingBehavior m_motionBehavior;
    ExpressionSmileBehavior m_expressionBehavior;

    bool m_initialized;
};

}
