#include "ExpressionCloseEyesBehavior.h"

#include "../../../expression/ExpressionManager.h"

namespace Ratuber
{

    ExpressionCloseEyesBehavior::ExpressionCloseEyesBehavior()
        :
        m_expressionManager(nullptr)
    {}

    ExpressionCloseEyesBehavior::~ExpressionCloseEyesBehavior()
    {}

    void ExpressionCloseEyesBehavior::Initialize(
        ExpressionManager* expressionManager
    )
    {
        m_expressionManager = expressionManager;

        if (!m_expressionManager)
        {
            return;
        }

        m_expressionManager->PlayExpression(
            "exp_03"
        );
    }

    void ExpressionCloseEyesBehavior::Update(
        float deltaTime
    )
    {
        // 当前基础表情行为不需要持续更新
    }

}