#pragma once
#include <string>
#include <unordered_map>


#include "CubismFramework.hpp"
#include "CubismModel.hpp"

#include "Motion/CubismMotionQueueManager.hpp"
#include "Motion/ACubismMotion.hpp"
#include "Motion/CubismMotionManager.hpp"

namespace Ratuber
{
    using namespace Live2D::Cubism::Framework;

    class MotionManager
    {


    public:
        MotionManager();


        ~MotionManager();

        // 绑定模型

        void SetModel(
            CubismModel* model
        );

        // 加载 motion3.json

        bool LoadMotion(
            const std::string& id,
            const std::string& filePath
        );

        // 播放动作

        bool PlayMotion(
            const std::string& id,
            int priority = 2
        );

        // 停止动作

        void StopMotion();
        // 每帧更新

        void Update(
            float deltaTimeSeconds
        );
        // 是否正在播放

        bool IsPlaying() const;

        // 当前动作ID

        const std::string&
            GetCurrentMotionId() const;

        // 是否存在动作

        bool HasMotion(
            const std::string& id
        ) const;
        // 清理

        void Clear();

    private:
        struct MotionData
        {

            ACubismMotion* motion = nullptr;

        };

    private:

        CubismModel* _model;
        // Cubism 5 动作管理器

        CubismMotionManager*
            _motionManager;

        // 当前动作句柄

        CubismMotionQueueEntryHandle
            _currentMotionHandle;

        // 当前动作名字

        std::string
            _currentMotionId;
        // 所有动作

        std::unordered_map<
            std::string,
            MotionData
        >
            _motions;
        static const std::string
            _emptyMotionId;
    };
}