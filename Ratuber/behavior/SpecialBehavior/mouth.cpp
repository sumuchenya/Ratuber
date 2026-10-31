#include "mouth.h"

#include "../../MouthOpen/MouthOpenManager.h"

#include <cmath>

namespace
{
    // 嘴巴开合的主速度（弧度/秒）。
    // 16 大约每秒开合 2.5 次，接近正常语速；觉得慢就调大，觉得太快就调小。
    constexpr float kMainSpeed = 16.0f;

    // 叠加一个更快的小波动，让开合节奏不那么机械。
    constexpr float kDetailSpeed = 23.0f;

    // 小波动占的比例（0 = 不叠加）。
    constexpr float kDetailWeight = 0.3f;
}

namespace Ratuber
{
    Mouth::Mouth()
        :
        m_mouthOpenManager(nullptr),
        m_time(0.0f)
    {}

    Mouth::~Mouth()
    {}

    void Mouth::Initialize(MouthOpenManager* mouthOpenManager)
    {
        m_mouthOpenManager = mouthOpenManager;
        m_time = 0.0f;
    }

    void Mouth::Update(float deltaTime)
    {
        if (!m_mouthOpenManager)
        {
            return;
        }

        m_time += deltaTime;

        // 主波 + 小波动，范围在 -1 到 1 之间。
        const float wave =
            (1.0f - kDetailWeight) * std::sin(m_time * kMainSpeed)
            + kDetailWeight * std::sin(m_time * kDetailSpeed + 1.3f);

        // 映射到 0（闭）到 1（全开）。
        const float mouth =
            (wave + 1.0f) * 0.5f;

        m_mouthOpenManager->SetMouthOpen(mouth);
    }
}