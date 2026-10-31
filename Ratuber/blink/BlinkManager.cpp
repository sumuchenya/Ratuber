#include "BlinkManager.h"

#include <algorithm>

#include "CubismFramework.hpp"
#include "CubismModel.hpp"
#include "Id/CubismIdManager.hpp"

namespace Ratuber
{

    BlinkManager::BlinkManager()
        :
        _model(nullptr),
        _eyeOpen(1.0f)
    {}


    BlinkManager::~BlinkManager()
    {
        _model = nullptr;
    }


    void BlinkManager::SetModel(
        Live2D::Cubism::Framework::CubismModel* model
    )
    {
        _model = model;

        if (_model)
        {
            SetEyeOpen(_eyeOpen);
        }
    }


    void BlinkManager::SetEyeOpen(float value)
    {
        _eyeOpen = std::clamp(
            value,
            0.0f,
            1.0f
        );

        if (!_model)
        {
            return;
        }

        using namespace Live2D::Cubism::Framework;

        // ========================================================
        // 左眼
        // ========================================================

        auto leftEyeId =
            CubismFramework::GetIdManager()->GetId(
                LEFT_EYE_PARAMETER
            );

        const int leftEyeIndex =
            _model->GetParameterIndex(leftEyeId);

        if (leftEyeIndex >= 0)
        {
            _model->SetParameterValue(
                leftEyeIndex,
                _eyeOpen
            );
        }


        // ========================================================
        // 右眼
        // ========================================================

        auto rightEyeId =
            CubismFramework::GetIdManager()->GetId(
                RIGHT_EYE_PARAMETER
            );

        const int rightEyeIndex =
            _model->GetParameterIndex(rightEyeId);

        if (rightEyeIndex >= 0)
        {
            _model->SetParameterValue(
                rightEyeIndex,
                _eyeOpen
            );
        }
    }


    void BlinkManager::CloseEyes()
    {
        SetEyeOpen(0.0f);
    }


    void BlinkManager::OpenEyes()
    {
        SetEyeOpen(1.0f);
    }


    void BlinkManager::Update(float deltaTimeSeconds)
    {
        (void)deltaTimeSeconds;

        // 每帧在动作之后重新应用当前值，
        // 保证动作播放期间眨眼仍然生效。
        SetEyeOpen(_eyeOpen);
    }


    float BlinkManager::GetEyeOpen() const
    {
        return _eyeOpen;
    }

}

