#pragma once

namespace Ratuber
{

    class ExpressionManager;

    class ExpressionSmileBehavior
    {
    public:
        ExpressionSmileBehavior();
        ~ExpressionSmileBehavior();

        void Initialize(ExpressionManager* expressionManager);
        void Update(float deltaTime);

    private:
        ExpressionManager* m_expressionManager;
    };

}