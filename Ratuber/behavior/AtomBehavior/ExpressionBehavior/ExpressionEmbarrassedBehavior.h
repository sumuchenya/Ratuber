#pragma once

namespace Ratuber
{

    class ExpressionManager;

    class ExpressionEmbarrassedBehavior
    {
    public:
        ExpressionEmbarrassedBehavior();
        ~ExpressionEmbarrassedBehavior();

        void Initialize(ExpressionManager* expressionManager);
        void Update(float deltaTime);

    private:
        ExpressionManager* m_expressionManager;
    };

}