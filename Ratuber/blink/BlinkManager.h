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

    class BlinkManager
    {

    public:

        BlinkManager();

        ~BlinkManager();


        void SetModel(
            Live2D::Cubism::Framework::CubismModel* model
        );


        void SetEyeOpen(
            float value
        );


        void CloseEyes();

        void OpenEyes();


        void Update(
            float deltaTimeSeconds
        );


        float GetEyeOpen() const;


    private:

        Live2D::Cubism::Framework::CubismModel* _model;

        float _eyeOpen;


        static constexpr const char*
            LEFT_EYE_PARAMETER =
            "ParamEyeLOpen";


        static constexpr const char*
            RIGHT_EYE_PARAMETER =
            "ParamEyeROpen";

    };

}