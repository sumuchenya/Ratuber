#include "MouthOpenManager.h"

#include <algorithm>

#include "CubismFramework.hpp"
#include "CubismModel.hpp"
#include "Id/CubismIdManager.hpp"

namespace Ratuber
{

    MouthOpenManager::MouthOpenManager()
        :
        _model(nullptr),
        _mouthOpen(0.0f)
    {}


    MouthOpenManager::~MouthOpenManager()
    {
        _model = nullptr;
    }


    void MouthOpenManager::SetModel(
        Live2D::Cubism::Framework::CubismModel* model
    )
    {
        _model = model;

        if (_model)
        {
            SetMouthOpen(_mouthOpen);
        }
    }


    void MouthOpenManager::SetMouthOpen(float value)
    {
        _mouthOpen =
            std::clamp(
                value,
                0.0f,
                1.0f
            );


        if (!_model)
        {
            return;
        }


        using namespace Live2D::Cubism::Framework;


        CubismIdHandle mouthId =
            CubismFramework::GetIdManager()->GetId(
                MOUTH_PARAMETER
            );


        const csmInt32 mouthIndex =
            _model->GetParameterIndex(
                mouthId
            );


        if (mouthIndex >= 0)
        {
            _model->SetParameterValue(
                mouthIndex,
                _mouthOpen
            );
        }
    }


    void MouthOpenManager::CloseMouth()
    {
        SetMouthOpen(0.0f);
    }


    void MouthOpenManager::OpenMouth()
    {
        SetMouthOpen(1.0f);
    }


    void MouthOpenManager::Update(float deltaTimeSeconds)
    {
        (void)deltaTimeSeconds;

        // 每帧在动作之后重新应用当前值，
        // 保证动作播放期间张嘴仍然生效。
        SetMouthOpen(_mouthOpen);
    }

    float MouthOpenManager::GetMouthOpen() const
    {
        return _mouthOpen;
    }

}