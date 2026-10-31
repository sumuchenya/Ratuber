#pragma once

namespace Ratuber
{

    class ExpressionManager;

    class ExpressionSurprisedBehavior
    {
    public:
        ExpressionSurprisedBehavior();
        ~ExpressionSurprisedBehavior();

        void Initialize(ExpressionManager* expressionManager);
        void Update(float deltaTime);

    private:
        ExpressionManager* m_expressionManager;
    };

}