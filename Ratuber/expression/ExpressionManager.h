#pragma once

#include <string>
#include <unordered_map>

namespace Live2D
{
    namespace Cubism
    {
        namespace Framework
        {
            class CubismModel;
            class CubismExpressionMotion;
            class CubismMotionManager;
        }
    }
}

namespace Ratuber
{

    class ExpressionManager
    {

    public:

        ExpressionManager();

        ~ExpressionManager();


        // 绑定 Live2D模型

        void SetModel(
            Live2D::Cubism::Framework::CubismModel* model
        );


        // 加载表情文件

        bool LoadExpression(
            const std::string& id,
            const std::string& filePath
        );


        // 播放表情

        bool PlayExpression(
            const std::string& id
        );


        // 停止表情

        void StopExpression();


        // 每帧更新

        void Update(
            float deltaTimeSeconds
        );


        // 当前表情

        const std::string&
            GetCurrentExpressionId() const;


        // 是否存在

        bool HasExpression(
            const std::string& id
        ) const;


        // 清理

        void Clear();


    private:


        struct ExpressionData
        {

            Live2D::Cubism::Framework::
                CubismExpressionMotion*
                expression = nullptr;

        };


    private:


        // Live2D模型

        Live2D::Cubism::Framework::
            CubismModel*
            _model;



        // Cubism5:
        // Expression 通过 MotionManager 管理

        Live2D::Cubism::Framework::
            CubismMotionManager*
            _motionManager;



        // 所有表情

        std::unordered_map<
            std::string,
            ExpressionData
        >
            _expressions;



        // 当前表情ID

        std::string
            _currentExpressionId;



        static const std::string
            _emptyExpressionId;

    };

}