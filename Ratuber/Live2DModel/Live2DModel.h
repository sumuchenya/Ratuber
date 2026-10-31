#pragma once

#include <SFML/Graphics.hpp>

namespace Live2D
{
    namespace Cubism
    {
        namespace Framework
        {
            class CubismModel;
            class CubismMoc;
            class CubismModelSettingJson;
            class CubismModelMatrix;
            class CubismPhysics;

            namespace Rendering
            {
                class CubismRenderer;
            }
        }
    }
}

class Live2DModel
{
public:
    Live2DModel();
    ~Live2DModel();

    Live2DModel(const Live2DModel&) = delete;
    Live2DModel& operator=(const Live2DModel&) = delete;

    bool Initialize();

    void SetModel(
        Live2D::Cubism::Framework::CubismModel* model
    );

    void SetScale(float scale);

    void Update(float deltaTime);

    void Render(
        unsigned int width = 800,
        unsigned int height = 1000
    );

    void Shutdown();

    Live2D::Cubism::Framework::CubismModel* GetModel();

private:
    Live2D::Cubism::Framework::CubismModel* _model = nullptr;

    Live2D::Cubism::Framework::CubismMoc* _moc = nullptr;

    Live2D::Cubism::Framework::CubismModelSettingJson* _setting = nullptr;

    Live2D::Cubism::Framework::Rendering::CubismRenderer* _renderer = nullptr;

    Live2D::Cubism::Framework::CubismModelMatrix* _modelMatrix = nullptr;

    // ========================================================
    // Cubism Physics
    // ========================================================
    Live2D::Cubism::Framework::CubismPhysics* _physics = nullptr;

    sf::Texture _texture;
};