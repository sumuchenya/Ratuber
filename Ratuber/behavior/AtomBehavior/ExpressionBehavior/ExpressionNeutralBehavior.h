#pragma once

namespace Ratuber
{

    class ExpressionManager;

    class ExpressionNeutralBehavior
    {
    public:
        ExpressionNeutralBehavior();
        ~ExpressionNeutralBehavior();

        void Initialize(ExpressionManager* expressionManager);
        void Update(float deltaTime);

    private:
        ExpressionManager* m_expressionManager;
    };

}