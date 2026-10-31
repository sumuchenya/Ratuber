#pragma once

namespace Ratuber
{

    class ExpressionManager;

    class ExpressionSadBehavior
    {
    public:
        ExpressionSadBehavior();
        ~ExpressionSadBehavior();

        void Initialize(ExpressionManager* expressionManager);
        void Update(float deltaTime);

    private:
        ExpressionManager* m_expressionManager;
    };

}