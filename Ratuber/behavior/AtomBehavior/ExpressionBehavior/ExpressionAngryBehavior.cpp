#include "ExpressionAngryBehavior.h"

#include "../../../expression/ExpressionManager.h"

namespace Ratuber
{

    ExpressionAngryBehavior::ExpressionAngryBehavior()
        :
        m_expressionManager(nullptr)
    {}

    ExpressionAngryBehavior::~ExpressionAngryBehavior()
    {}

    void ExpressionAngryBehavior::Initialize(
        ExpressionManager* expressionManager
    )
    {
        m_expressionManager = expressionManager;

        if (!m_expressionManager)
        {
            return;
        }

        m_expressionManager->PlayExpression(
            "exp_08"
        );
    }

    void ExpressionAngryBehavior::Update(
        float deltaTime
    )
    {
        // 当前基础表情行为不需要持续更新
    }

}