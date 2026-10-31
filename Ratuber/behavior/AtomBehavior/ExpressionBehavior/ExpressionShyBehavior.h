#pragma once

namespace Ratuber
{

    class ExpressionManager;

    class ExpressionShyBehavior
    {
    public:
        ExpressionShyBehavior();
        ~ExpressionShyBehavior();

        void Initialize(ExpressionManager* expressionManager);
        void Update(float deltaTime);

    private:
        ExpressionManager* m_expressionManager;
    };

}