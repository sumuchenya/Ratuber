#pragma once

#include "../AtomBehavior/MotionBehavior/MotionHatBehavior.h"
#include "../AtomBehavior/ExpressionBehavior/ExpressionSmileBehavior.h"
#include "../AtomBehavior/ExpressionBehavior/ExpressionShyBehavior.h"

namespace Ratuber
{

class MotionManager;
class ExpressionManager;

class ShyBehavior
{
public:
    ShyBehavior();
    ~ShyBehavior();

    void Initialize(
        MotionManager* motionManager,
        ExpressionManager* expressionManager
    );

    void Update(float deltaTime);

private:
    MotionHatBehavior m_motionBehavior;

    ExpressionSmileBehavior m_smileBehavior;
    ExpressionShyBehavior m_shyBehavior;

    bool m_initialized;
};

}
