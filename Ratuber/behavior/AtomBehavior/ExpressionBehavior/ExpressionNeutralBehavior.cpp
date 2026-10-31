#include "ExpressionNeutralBehavior.h"

#include "../../../expression/ExpressionManager.h"

namespace Ratuber
{

    ExpressionNeutralBehavior::ExpressionNeutralBehavior()
        :
        m_expressionManager(nullptr)
    {}

    ExpressionNeutralBehavior::~ExpressionNeutralBehavior()
    {}

    void ExpressionNeutralBehavior::Initialize(
        ExpressionManager* expressionManager
    )
    {
        m_expressionManager = expressionManager;

        if (!m_expressionManager)
        {
            return;
        }

        m_expressionManager->PlayExpression(
            "exp_01"
        );
    }

    void ExpressionNeutralBehavior::Update(
        float deltaTime
    )
    {
        // 当前基础表情行为不需要持续更新
    }

}