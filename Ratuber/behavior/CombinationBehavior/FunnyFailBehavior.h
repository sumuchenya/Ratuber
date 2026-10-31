#pragma once

#include "../AtomBehavior/MotionBehavior/MotionMagicFailBehavior.h"
#include "../AtomBehavior/ExpressionBehavior/ExpressionEmbarrassedBehavior.h"

namespace Ratuber
{

class MotionManager;
class ExpressionManager;

class FunnyFailBehavior
{
public:
    FunnyFailBehavior();
    ~FunnyFailBehavior();

    void Initialize(
        MotionManager* motionManager,
        ExpressionManager* expressionManager
    );

    void Update(float deltaTime);

private:
    MotionMagicFailBehavior m_motionBehavior;
    ExpressionEmbarrassedBehavior m_expressionBehavior;

    bool m_initialized;
};

}
