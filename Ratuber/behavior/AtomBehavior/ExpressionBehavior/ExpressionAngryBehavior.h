#pragma once

namespace Ratuber
{

    class ExpressionManager;

    class ExpressionAngryBehavior
    {
    public:
        ExpressionAngryBehavior();
        ~ExpressionAngryBehavior();

        void Initialize(ExpressionManager* expressionManager);
        void Update(float deltaTime);

    private:
        ExpressionManager* m_expressionManager;
    };

}