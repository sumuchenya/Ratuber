#pragma once

namespace Ratuber
{
    class MouthOpenManager;

    class Mouth
    {
    public:
        Mouth();
        ~Mouth();

        void Initialize(MouthOpenManager* mouthOpenManager);
        void Update(float deltaTime);

    private:
        MouthOpenManager* m_mouthOpenManager;
        float m_time;
    };
}
