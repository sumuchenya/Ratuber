#include "blink.h"

#include "../../blink/BlinkManager.h"

#include <cmath>

namespace Ratuber
{
    Blink::Blink()
        :
        m_blinkManager(nullptr),
        m_time(0.0f)
    {}

    Blink::~Blink()
    {}

    void Blink::Initialize(BlinkManager* blinkManager)
    {
        m_blinkManager = blinkManager;
        m_time = 0.0f;
    }

    void Blink::Update(float deltaTime)
    {
        if (!m_blinkManager)
        {
            return;
        }

        m_time += deltaTime;

        const float blinkTime = std::fmod(m_time, 4.0f);

        if (blinkTime >= 3.80f && blinkTime < 3.95f)
        {
            m_blinkManager->SetEyeOpen(0.0f);
        }
        else
        {
            m_blinkManager->SetEyeOpen(1.0f);
        }
    }
}

