#include "ExpressionSmileBehavior.h"

#include "../../../expression/ExpressionManager.h"

namespace Ratuber
{

    ExpressionSmileBehavior::ExpressionSmileBehavior()
        :
        m_expressionManager(nullptr)
    {}

    ExpressionSmileBehavior::~ExpressionSmileBehavior()
    {}

    void ExpressionSmileBehavior::Initialize(
        ExpressionManager* expressionManager
    )
    {
        m_expressionManager = expressionManager;

        if (!m_expressionManager)
        {
            return;
        }

        m_expressionManager->PlayExpression(
            "exp_02"
        );
    }

    void ExpressionSmileBehavior::Update(
        float deltaTime
    )
    {
        // 当前基础表情行为不需要持续更新
    }

}