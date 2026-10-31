#pragma once

#include "../AtomBehavior/ExpressionBehavior/ExpressionSadBehavior.h"

namespace Ratuber
{

class ExpressionManager;

class SadBehavior
{
public:
    SadBehavior();
    ~SadBehavior();

    void Initialize(ExpressionManager* expressionManager);

    void Update(float deltaTime);

private:
    ExpressionSadBehavior m_expressionBehavior;

    bool m_initialized;
};

}
