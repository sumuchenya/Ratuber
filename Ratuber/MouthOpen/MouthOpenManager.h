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

    class MouthOpenManager
    {
    public:
        MouthOpenManager();
        ~MouthOpenManager();

        // 绑定 Live2D 模型
        void SetModel(Live2D::Cubism::Framework::CubismModel* model);

        // 设置嘴巴开合度
        // 0.0f = 闭嘴
        // 1.0f = 最大程度张嘴
        void SetMouthOpen(float value);

        // 闭嘴
        void CloseMouth();

        // 张嘴
        void OpenMouth();

        // 更新嘴巴状态
        void Update(float deltaTimeSeconds);

        // 获取当前嘴巴开合度
        float GetMouthOpen() const;

    private:
        Live2D::Cubism::Framework::CubismModel* _model;

        float _mouthOpen;

        static constexpr const char* MOUTH_PARAMETER = "ParamA";
    };

}