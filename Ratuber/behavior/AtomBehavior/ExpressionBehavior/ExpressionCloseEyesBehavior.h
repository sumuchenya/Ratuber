#pragma once

namespace Ratuber
{

    class ExpressionManager;

    class ExpressionCloseEyesBehavior
    {
    public:
        ExpressionCloseEyesBehavior();
        ~ExpressionCloseEyesBehavior();

        void Initialize(ExpressionManager* expressionManager);
        void Update(float deltaTime);

    private:
        ExpressionManager* m_expressionManager;
    };

}