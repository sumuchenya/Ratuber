#include "ExpressionSurprisedBehavior.h"

#include "../../../expression/ExpressionManager.h"

namespace Ratuber
{

    ExpressionSurprisedBehavior::ExpressionSurprisedBehavior()
        :
        m_expressionManager(nullptr)
    {}

    ExpressionSurprisedBehavior::~ExpressionSurprisedBehavior()
    {}

    void ExpressionSurprisedBehavior::Initialize(
        ExpressionManager* expressionManager
    )
    {
        m_expressionManager = expressionManager;

        if (!m_expressionManager)
        {
            return;
        }

        m_expressionManager->PlayExpression(
            "exp_04"
        );
    }

    void ExpressionSurprisedBehavior::Update(
        float deltaTime
    )
    {
        // 当前基础表情行为不需要持续更新
    }

}