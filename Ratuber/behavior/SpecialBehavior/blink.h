#pragma once

namespace Ratuber
{
    class BlinkManager;

    class Blink
    {
    public:
        Blink();
        ~Blink();

        void Initialize(BlinkManager* blinkManager);
        void Update(float deltaTime);

    private:
        BlinkManager* m_blinkManager;
        float m_time;
    };
}
