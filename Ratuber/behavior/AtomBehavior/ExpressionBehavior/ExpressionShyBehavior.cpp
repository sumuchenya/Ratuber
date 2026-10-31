#include "ExpressionShyBehavior.h"

#include "../../../expression/ExpressionManager.h"

namespace Ratuber
{

    ExpressionShyBehavior::ExpressionShyBehavior()
        :
        m_expressionManager(nullptr)
    {}

    ExpressionShyBehavior::~ExpressionShyBehavior()
    {}

    void ExpressionShyBehavior::Initialize(
        ExpressionManager* expressionManager
    )
    {
        m_expressionManager = expressionManager;

        if (!m_expressionManager)
        {
            return;
        }

        m_expressionManager->PlayExpression(
            "exp_06"
        );
    }

    void ExpressionShyBehavior::Update(
        float deltaTime
    )
    {
        // 当前基础表情行为不需要持续更新
    }

}