#include "MotionManager.h"
#include <fstream>
#include <vector>
#include "CubismModel.hpp"
#include "Motion/CubismMotion.hpp"
#include "Motion/ACubismMotion.hpp"
#include "Motion/CubismMotionManager.hpp"
#include <iostream>
using namespace Live2D::Cubism::Framework;
namespace Ratuber
{
    const std::string
        MotionManager::_emptyMotionId = "";
    MotionManager::MotionManager()

        :
        _model(nullptr),

        _motionManager(
            new CubismMotionManager()
        ),

        _currentMotionHandle(
            InvalidMotionQueueEntryHandleValue
        )

    {

    }
    MotionManager::~MotionManager()

    {

        Clear();


        delete _motionManager;

        _motionManager = nullptr;

    }
    void MotionManager::SetModel(
        CubismModel* model
    )

    {

        _model = model;

    }
    bool MotionManager::LoadMotion(
        const std::string& id,
        const std::string& filePath
    )

    {

        std::ifstream file(
            filePath,
            std::ios::binary |
            std::ios::ate
        );



        if (!file.is_open())
        {
            return false;
        }

        const std::streamsize size =
            file.tellg();



        if (size <= 0)
        {
            return false;
        }




        file.seekg(
            0,
            std::ios::beg
        );




        std::vector<csmByte> buffer(
            static_cast<size_t>(size)
        );




        if (!file.read(
            reinterpret_cast<char*>(
                buffer.data()
                ),
            size
        ))
        {
            return false;
        }
        CubismMotion* motion =
            CubismMotion::Create(
                buffer.data(),
                static_cast<csmSizeInt>(size)
            );
        if (!motion)
        {
            return false;
        }

        auto old =
            _motions.find(id);

        if (old != _motions.end())
        {

            if (old->second.motion)
            {
                ACubismMotion::Delete(
                    old->second.motion
                );
            }

        }

        MotionData data;

        data.motion = motion;


        _motions[id] = data;
        std::cout
            << "[Motion Loaded] "
            << id
            << std::endl;
        return true;
    }

    bool MotionManager::PlayMotion(

        const std::string& id,
        int priority
    )

    {

        if (!_model)
        {
            return false;
        }

        auto it =
            _motions.find(id);

        if (it == _motions.end())
        {
            std::cout
                << "[Motion Missing] "
                << id
                << std::endl;

            return false;
        }
        if (!it->second.motion)
        {
            return false;
        }
        CubismMotionQueueEntryHandle handle =
            _motionManager->StartMotionPriority(

                it->second.motion,
                false,
                priority
            );

        if (handle ==
            InvalidMotionQueueEntryHandleValue)
        {
            return false;
        }
        _currentMotionHandle = handle;


        _currentMotionId = id;

        return true;

    }

    void MotionManager::StopMotion()

    {

        if (_motionManager)
        {
            _motionManager->StopAllMotions();
        }



        _currentMotionHandle =
            InvalidMotionQueueEntryHandleValue;

        _currentMotionId.clear();

    }

    void MotionManager::Update(
        float deltaTimeSeconds
    )

    {

        if (!_model)
        {
            return;
        }

        _motionManager->UpdateMotion(
            _model,
            deltaTimeSeconds
        );

        if (_currentMotionId.empty())
        {
            return;
        }
        if (
            _motionManager->IsFinished(
                _currentMotionHandle
            )
            )

        {

            _currentMotionHandle =
                InvalidMotionQueueEntryHandleValue;



            _currentMotionId.clear();

        }


    }
    bool MotionManager::IsPlaying() const

    {

        return
            !_currentMotionId.empty();

    }
    const std::string&
        MotionManager::GetCurrentMotionId() const

    {

        if (_currentMotionId.empty())
        {
            return _emptyMotionId;
        }


        return _currentMotionId;

    }
    bool MotionManager::HasMotion(
        const std::string& id
    ) const

    {

        return
            _motions.find(id)
            != _motions.end();

    }
    void MotionManager::Clear()

    {

        if (_motionManager)
        {
            _motionManager->StopAllMotions();
        }

        _currentMotionHandle =
            InvalidMotionQueueEntryHandleValue;
        _currentMotionId.clear();

        for (auto& item : _motions)

        {

            if (item.second.motion)
            {

                ACubismMotion::Delete(
                    item.second.motion
                );


                item.second.motion = nullptr;

            }

        }
        _motions.clear();

    }
}