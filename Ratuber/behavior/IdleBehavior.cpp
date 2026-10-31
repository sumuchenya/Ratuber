#include "IdleBehavior.h"

#include "SpecialBehavior/blink.h"
#include "SpecialBehavior/mouth.h"
#include "SpecialBehavior/shake.h"

#include "CombinationBehavior/FunnyFailBehavior.h"

#include "../motion/MotionManager.h"
#include "../expression/ExpressionManager.h"
#include "../blink/BlinkManager.h"
#include "../MouthOpen/MouthOpenManager.h"

namespace Ratuber
{
    IdleBehavior::IdleBehavior()
        :
        m_motionManager(nullptr),
        m_blinkManager(nullptr),
        m_mouthOpenManager(nullptr),
        m_model(nullptr),
        m_shakeReady(false),
        m_initialized(false)
    {}

    IdleBehavior::~IdleBehavior() = default;

    void IdleBehavior::SetModel(
        Live2D::Cubism::Framework::CubismModel* model
    )
    {
        m_model = model;
        m_shakeReady = false;
    }

    void IdleBehavior::Initialize(
        MotionManager* motionManager,
        ExpressionManager* expressionManager,
        BlinkManager* blinkManager,
        MouthOpenManager* mouthOpenManager
    )
    {
        m_motionManager = motionManager;
        m_blinkManager = blinkManager;
        m_mouthOpenManager = mouthOpenManager;
        m_shakeReady = false;
        m_initialized = false;

        m_blink = std::make_unique<Blink>();
        m_mouth = std::make_unique<Mouth>();
        m_shake = std::make_unique<Shake>();
        m_funnyFailBehavior = std::make_unique<FunnyFailBehavior>();

        if (m_blinkManager)
        {
            m_blink->Initialize(m_blinkManager);
        }

        if (m_mouthOpenManager)
        {
            m_mouth->Initialize(m_mouthOpenManager);
        }

        // 组合行为：动作和表情同时触发一次。
        if (motionManager && expressionManager)
        {
            m_funnyFailBehavior->Initialize(
                motionManager,
                expressionManager
            );
        }

        m_initialized = true;
    }

    void IdleBehavior::Update(float deltaTime)
    {
        if (!m_initialized)
        {
            return;
        }

        // 组合行为自带 Update，每帧调用。
        m_funnyFailBehavior->Update(deltaTime);

        // 晃动：main.cpp 先 Initialize 后 SetModel，
        // 所以等模型到位后再初始化 Shake。
        if (m_model)
        {
            if (!m_shakeReady)
            {
                m_shake->Initialize(m_model);
                m_shakeReady = true;
            }

            // 动作播放期间不调用晃动，动作自带头部和身体运动。
            const bool motionPlaying =
                m_motionManager && m_motionManager->IsPlaying();

            if (!motionPlaying)
            {
                m_shake->Update(deltaTime);
            }
        }

        // 眨眼
        if (m_blinkManager)
        {
            m_blink->Update(deltaTime);
        }

        // 张嘴
        if (m_mouthOpenManager)
        {
            m_mouth->Update(deltaTime);
        }
    }
}