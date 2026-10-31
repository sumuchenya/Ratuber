#include "ExpressionManager.h"

#include <fstream>
#include <vector>

#include "CubismModel.hpp"

#include "Motion/CubismExpressionMotion.hpp"
#include "Motion/CubismMotionManager.hpp"
#include "Motion/ACubismMotion.hpp"


using namespace Live2D::Cubism::Framework;


namespace Ratuber
{

    const std::string ExpressionManager::_emptyExpressionId = "";


    ExpressionManager::ExpressionManager()
        :
        _model(nullptr),
        _motionManager(
            new CubismMotionManager()
        )
    {

    }


    ExpressionManager::~ExpressionManager()
    {
        Clear();

        delete _motionManager;

        _motionManager = nullptr;
    }



    void ExpressionManager::SetModel(
        CubismModel* model
    )
    {
        _model = model;
    }



    bool ExpressionManager::LoadExpression(
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



        CubismExpressionMotion* expression =
            CubismExpressionMotion::Create(
                buffer.data(),
                static_cast<csmSizeInt>(size)
            );


        if (!expression)
        {
            return false;
        }



        auto old =
            _expressions.find(id);


        if (old != _expressions.end())
        {
            if (old->second.expression)
            {
                ACubismMotion::Delete(
                    old->second.expression
                );
            }
        }



        ExpressionData data;

        data.expression = expression;


        _expressions[id] = data;


        return true;
    }




    bool ExpressionManager::PlayExpression(
        const std::string& id
    )
    {
        if (!_model)
        {
            return false;
        }

        auto it =
            _expressions.find(id);

        if (it == _expressions.end())
        {
            return false;
        }

        if (!it->second.expression)
        {
            return false;
        }


        _motionManager->StartMotion(
            it->second.expression,
            false
        );


        _currentExpressionId = id;


        return true;
    }



    void ExpressionManager::StopExpression()
    {
        if (_motionManager)
        {
            _motionManager->StopAllMotions();
        }


        _currentExpressionId.clear();
    }




    void ExpressionManager::Update(
        float deltaTimeSeconds
    )
    {
        if (!_model)
        {
            return;
        }


        if (!_motionManager)
        {
            return;
        }


        _motionManager->UpdateMotion(
            _model,
            deltaTimeSeconds
        );
    }




    const std::string&
        ExpressionManager::GetCurrentExpressionId() const
    {
        if (_currentExpressionId.empty())
        {
            return _emptyExpressionId;
        }


        return _currentExpressionId;
    }




    bool ExpressionManager::HasExpression(
        const std::string& id
    ) const
    {
        return
            _expressions.find(id)
            != _expressions.end();
    }




    void ExpressionManager::Clear()
    {
        if (_motionManager)
        {
            _motionManager->StopAllMotions();
        }



        for (auto& item : _expressions)
        {
            if (item.second.expression)
            {
                ACubismMotion::Delete(
                    item.second.expression
                );

                item.second.expression = nullptr;
            }
        }


        _expressions.clear();

        _currentExpressionId.clear();
    }



}