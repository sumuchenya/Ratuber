#include "ExpressionEmbarrassedBehavior.h"

#include "../../../expression/ExpressionManager.h"

namespace Ratuber
{

    ExpressionEmbarrassedBehavior::ExpressionEmbarrassedBehavior()
        :
        m_expressionManager(nullptr)
    {}

    ExpressionEmbarrassedBehavior::~ExpressionEmbarrassedBehavior()
    {}

    void ExpressionEmbarrassedBehavior::Initialize(
        ExpressionManager* expressionManager
    )
    {
        m_expressionManager = expressionManager;

        if (!m_expressionManager)
        {
            return;
        }

        m_expressionManager->PlayExpression(
            "exp_07"
        );
    }

    void ExpressionEmbarrassedBehavior::Update(
        float deltaTime
    )
    {
        // 当前基础表情行为不需要持续更新
    }

}