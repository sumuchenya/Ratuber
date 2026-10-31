#include "shake.h"

#include "CubismFramework.hpp"
#include "CubismModel.hpp"
#include "Id/CubismIdManager.hpp"

#include <cmath>
#include <algorithm>

namespace
{
    // 摇晃幅度的整体倍数（1.0 = 原来的大小）。
    constexpr float kAmplitudeScale = 1.8f;

    // 摇晃速度的整体倍数（1.0 = 原来的速度）。
    constexpr float kSpeedScale = 1.6f;
}

namespace Ratuber
{
    using namespace Live2D::Cubism::Framework;

    Shake::Shake()
        :
        m_model(nullptr),
        m_time(0.0f)
    {}

    Shake::~Shake()
    {}

    void Shake::Initialize(CubismModel* model)
    {
        m_model = model;
        m_time = 0.0f;
    }

    void Shake::SetParameter(
        const char* parameterId,
        float value
    )
    {
        if (!m_model)
        {
            return;
        }

        auto id =
            CubismFramework::GetIdManager()->GetId(parameterId);

        const int index = m_model->GetParameterIndex(id);

        if (index < 0)
        {
            return;
        }

        const float minValue =
            m_model->GetParameterMinimumValue(index);

        const float maxValue =
            m_model->GetParameterMaximumValue(index);

        value = std::clamp(value, minValue, maxValue);

        m_model->SetParameterValue(index, value);
    }

    void Shake::Update(float deltaTime)
    {
        if (!m_model)
        {
            return;
        }

        m_time += deltaTime * kSpeedScale;

        // 头部自然运动：公式不变，整体放大
        const float headX =
            std::sin(m_time * 0.55f) * 6.0f * kAmplitudeScale;

        const float headY =
            std::sin(m_time * 0.37f + 1.2f) * 4.0f * kAmplitudeScale;

        const float headZ =
            std::sin(m_time * 0.43f + 2.0f) * 3.0f * kAmplitudeScale;

        SetParameter("ParamAngleX", headX);
        SetParameter("ParamAngleY", headY);
        SetParameter("ParamAngleZ", headZ);

        // 身体自然运动：公式不变，整体放大
        const float bodyX =
            std::sin(m_time * 0.31f + 0.7f) * 3.0f * kAmplitudeScale;

        const float bodyY =
            std::sin(m_time * 0.23f + 1.8f) * 2.0f * kAmplitudeScale;

        const float bodyZ =
            std::sin(m_time * 0.27f + 2.5f) * 2.0f * kAmplitudeScale;

        SetParameter("ParamBodyAngleX", bodyX);
        SetParameter("ParamBodyAngleY", bodyY);
        SetParameter("ParamBodyAngleZ", bodyZ);
    }
}