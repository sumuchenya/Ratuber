#pragma once

namespace Live2D
{
    namespace Cubism
    {
        namespace Framework
        {
            class CubismModel;
        }
    }
}

namespace Ratuber
{
    class Shake
    {
    public:
        Shake();
        ~Shake();

        void Initialize(
            Live2D::Cubism::Framework::CubismModel* model
        );

        void Update(float deltaTime);

    private:
        void SetParameter(const char* parameterId, float value);

        Live2D::Cubism::Framework::CubismModel* m_model;
        float m_time;
    };
}
