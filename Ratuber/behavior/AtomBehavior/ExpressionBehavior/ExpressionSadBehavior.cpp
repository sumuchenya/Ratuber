#include "ExpressionSadBehavior.h"

#include "../../../expression/ExpressionManager.h"

namespace Ratuber
{

    ExpressionSadBehavior::ExpressionSadBehavior()
        :
        m_expressionManager(nullptr)
    {}

    ExpressionSadBehavior::~ExpressionSadBehavior()
    {}

    void ExpressionSadBehavior::Initialize(
        ExpressionManager* expressionManager
    )
    {
        m_expressionManager = expressionManager;

        if (!m_expressionManager)
        {
            return;
        }

        m_expressionManager->PlayExpression(
            "exp_05"
        );
    }

    void ExpressionSadBehavior::Update(
        float deltaTime
    )
    {
        // 当前基础表情行为不需要持续更新
    }

}