#include "Live2DModel.h"

#include <fstream>
#include <string>
#include <cstdlib>
#include <cstdio>

#include "Math/CubismMatrix44.hpp"
#include "CubismFramework.hpp"
#include "CubismModel.hpp"
#include "CubismModelSettingJson.hpp"
#include "CubismMoc.hpp"
#include "Physics/CubismPhysics.hpp"
#include "Rendering/CubismRenderer.hpp"
#include "Math/CubismModelMatrix.hpp"

using namespace Live2D::Cubism::Framework;
using namespace Live2D::Cubism::Framework::Rendering;

#include "Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp"


// ============================================================
// Cubism 内存分配器
// ============================================================

class RatuberAllocator : public ICubismAllocator
{
public:

    void* Allocate(const csmSizeType size) override
    {
        return std::malloc(size);
    }

    void Deallocate(void* memory) override
    {
        std::free(memory);
    }

    void* AllocateAligned(
        const csmSizeType size,
        const csmUint32 alignment) override
    {
#ifdef _MSC_VER
        return _aligned_malloc(size, alignment);
#else
        void* memory = nullptr;

        if (posix_memalign(
            &memory,
            alignment,
            size) != 0)
        {
            return nullptr;
        }

        return memory;
#endif
    }

    void DeallocateAligned(
        void* alignedMemory) override
    {
#ifdef _MSC_VER
        _aligned_free(alignedMemory);
#else
        std::free(alignedMemory);
#endif
    }
};


// ============================================================
// 全局 Cubism Allocator
// ============================================================

static RatuberAllocator g_allocator;


// ============================================================
// Cubism 日志
// ============================================================

static void CubismLog(
    const csmChar* message)
{
    (void)message;
}


// ============================================================
// 文件读取
// ============================================================

static csmByte* LoadFile(
    std::string filePath,
    csmSizeInt* outSize)
{
    std::ifstream file(
        filePath,
        std::ios::binary |
        std::ios::ate
    );

    if (!file.is_open())
    {
        *outSize = 0;

        return nullptr;
    }

    const std::streamsize size =
        file.tellg();

    if (size <= 0)
    {
        *outSize = 0;

        return nullptr;
    }

    file.seekg(
        0,
        std::ios::beg
    );

    auto* buffer =
        new csmByte[
            static_cast<size_t>(size)
        ];

    if (!file.read(
        reinterpret_cast<char*>(buffer),
        size))
    {
        delete[] buffer;

        *outSize = 0;

        return nullptr;
    }

    *outSize =
        static_cast<csmSizeInt>(size);

    return buffer;
}


// ============================================================
// 文件释放
// ============================================================

static void ReleaseFile(
    csmByte* buffer)
{
    delete[] buffer;
}


// ============================================================
// Live2DModel
// ============================================================

Live2DModel::Live2DModel()
{}


Live2DModel::~Live2DModel()
{
    Shutdown();
}


// ============================================================
// 初始化
// ============================================================

bool Live2DModel::Initialize()
{
    // --------------------------------------------------------
    // Cubism Framework 配置
    // --------------------------------------------------------

    CubismFramework::Option option;

    option.LogFunction = CubismLog;

    option.LoggingLevel =
        CubismFramework::Option::LogLevel_Info;

    option.LoadFileFunction =
        LoadFile;

    option.ReleaseBytesFunction =
        ReleaseFile;


    // --------------------------------------------------------
    // 启动 Cubism Framework
    // --------------------------------------------------------

    if (!CubismFramework::StartUp(
        &g_allocator,
        &option))
    {
        return false;
    }


    // --------------------------------------------------------
    // 初始化 Cubism Framework
    // --------------------------------------------------------

    CubismFramework::Initialize();


    // --------------------------------------------------------
    // Mao.model3.json
    // --------------------------------------------------------

    const std::string modelPath =
        std::string(
            RATUBER_MAO_ASSET_DIR
        )
        + "/Mao.model3.json";


    csmSizeInt jsonSize = 0;

    csmByte* jsonData =
        LoadFile(
            modelPath.c_str(),
            &jsonSize
        );


    if (!jsonData)
    {
        return false;
    }


    // --------------------------------------------------------
    // 创建 ModelSetting
    // --------------------------------------------------------

    auto* setting =
        new CubismModelSettingJson(
            jsonData,
            jsonSize
        );


    ReleaseFile(jsonData);


    if (!setting)
    {
        return false;
    }


    // --------------------------------------------------------
    // 获取 moc3 文件名
    // --------------------------------------------------------

    const csmChar* mocFileName =
        setting->GetModelFileName();


    if (!mocFileName ||
        mocFileName[0] == '\0')
    {
        delete setting;

        return false;
    }


    // --------------------------------------------------------
    // 加载 moc3
    // --------------------------------------------------------

    const std::string mocPath =
        std::string(
            RATUBER_MAO_ASSET_DIR
        )
        + "/"
        + mocFileName;


    csmSizeInt mocSize = 0;

    csmByte* mocData =
        LoadFile(
            mocPath.c_str(),
            &mocSize
        );


    if (!mocData)
    {
        delete setting;

        return false;
    }


    // --------------------------------------------------------
    // 创建 CubismMoc
    // --------------------------------------------------------

    CubismMoc* moc =
        CubismMoc::Create(
            mocData,
            mocSize
        );


    ReleaseFile(mocData);


    if (!moc)
    {
        delete setting;

        return false;
    }


    // --------------------------------------------------------
    // 创建 CubismModel
    // --------------------------------------------------------

    CubismModel* model =
        moc->CreateModel();


    if (!model)
    {
        CubismMoc::Delete(moc);
        delete setting;

        return false;
    }


    // --------------------------------------------------------
    // 保存 Mao 模型
    // --------------------------------------------------------

    _model = model;
    _moc = moc;
    _setting = setting;


    // ========================================================
    // 临时调试：打印 Mao 模型实际参数
    // ========================================================

    printf("\n========== Mao Parameters ==========\n");

    const int parameterCount =
        _model->GetParameterCount();

    printf(
        "Parameter Count: %d\n",
        parameterCount
    );

    for (int i = 0; i < parameterCount; ++i)
    {
        auto* parameterId =
            _model->GetParameterId(i);

        if (parameterId)
        {
            printf(
                "[%d] ID=%s | Value=%f | Min=%f | Max=%f | Default=%f\n",
                i,
                parameterId->GetString().GetRawString(),
                _model->GetParameterValue(i),
                _model->GetParameterMinimumValue(i),
                _model->GetParameterMaximumValue(i),
                _model->GetParameterDefaultValue(i)
            );
        }
    }

    printf("===================================\n\n");


    // ========================================================
    // 创建 Cubism Physics
    // ========================================================
    //
    // Mao.model3.json 中已经声明：
    //
    // "Physics": "Mao.physics3.json"
    //
    // 因此直接从 ModelSetting 获取物理文件名。
    //
    // 物理加载失败不会影响模型本身继续运行。
    // ========================================================

    const csmChar* physicsFileName =
        _setting->GetPhysicsFileName();

    if (physicsFileName &&
        physicsFileName[0] != '\0')
    {
        const std::string physicsPath =
            std::string(
                RATUBER_MAO_ASSET_DIR
            )
            + "/"
            + physicsFileName;

        csmSizeInt physicsSize = 0;

        csmByte* physicsData =
            LoadFile(
                physicsPath.c_str(),
                &physicsSize
            );

        if (physicsData)
        {
            _physics =
                CubismPhysics::Create(
                    physicsData,
                    physicsSize
                );

            ReleaseFile(physicsData);

            if (_physics)
            {
                printf(
                    "[Physics] Loaded: %s\n",
                    physicsFileName
                );
            }
            else
            {
                printf(
                    "[Physics] Failed to create physics: %s\n",
                    physicsFileName
                );
            }
        }
        else
        {
            printf(
                "[Physics] Failed to load file: %s\n",
                physicsPath.c_str()
            );
        }
    }
    else
    {
        printf(
            "[Physics] No physics file specified.\n"
        );
    }


    // ========================================================
    // 创建 Mao Renderer
    // ========================================================

    _renderer =
        CubismRenderer::Create(
            800,
            1000
        );

    if (!_renderer)
    {
        return false;
    }


    _renderer->Initialize(
        _model
    );


    // ========================================================
    // 设置遮罩缓冲大小（Mao 使用 masking）
    // ========================================================

    auto* rendererOpenGL =
        dynamic_cast<CubismRenderer_OpenGLES2*>(
            _renderer
            );

    if (!rendererOpenGL)
    {
        return false;
    }


    rendererOpenGL->SetDrawableClippingMaskBufferSize(
        2048,
        2048
    );


    _renderer->SetRenderTargetSize(
        800,
        1000
    );


    _modelMatrix =
        new CubismModelMatrix(
            _model->GetCanvasWidth(),
            _model->GetCanvasHeight()
        );


    _modelMatrix->SetWidth(
        0.8f
    );


    _modelMatrix->SetCenterPosition(
        0.0f,
        0.0f
    );


    // ========================================================
    // 组合 Projection × Model 得到 MVP
    // ========================================================

    CubismMatrix44 projection;


    // 800 × 1000 窗口的宽高比
    projection.Scale(
        1.0f,
        800.0f / 1000.0f
    );


    projection.MultiplyByMatrix(
        _modelMatrix
    );


    _renderer->SetMvpMatrix(
        &projection
    );


    // ========================================================
    // 加载 Mao 纹理
    // ========================================================

    const std::string texturePath =
        std::string(
            RATUBER_MAO_ASSET_DIR
        )
        + "/Mao.2048/texture_00.png";


    if (!_texture.loadFromFile(
        texturePath))
    {
        return false;
    }


    const GLuint textureId =
        static_cast<GLuint>(
            _texture.getNativeHandle()
            );


    glBindTexture(
        GL_TEXTURE_2D,
        textureId
    );


    rendererOpenGL->BindTexture(
        0,
        textureId
    );


    return true;
}


// ============================================================
// 更新
// ============================================================

void Live2DModel::Update(
    float deltaTime)
{
    if (!_model)
    {
        return;
    }


    // ========================================================
    // Cubism Physics
    // ========================================================
    //
    // 此时上层的：
    //
    // IdleBehavior
    // MotionManager
    // ExpressionManager
    // BlinkManager
    // MouthOpenManager
    //
    // 已经完成本帧参数修改。
    //
    // Physics 在这里根据当前模型参数计算：
    //
    // 头发
    // 衣服
    // 饰品
    // 其它 physics 参数
    //
    // 然后再执行 CubismModel::Update()。
    // ========================================================

    if (_physics)
    {
        _physics->Evaluate(
            _model,
            deltaTime
        );
    }


    // ========================================================
    // 原来的模型更新
    // ========================================================

    _model->Update();
}


// ============================================================
// 获取模型
// ============================================================

Live2D::Cubism::Framework::CubismModel*
Live2DModel::GetModel()
{
    return _model;
}


// ============================================================
// 缩放
// ============================================================

void Live2DModel::SetScale(
    float scale)
{
    _modelMatrix->SetWidth(
        0.8f * scale
    );
}


// ============================================================
// 渲染
// ============================================================

void Live2DModel::Render(
    unsigned int width,
    unsigned int height)
{
    if (!_renderer ||
        !_model ||
        width == 0 ||
        height == 0)
    {
        return;
    }


    glViewport(
        0,
        0,
        static_cast<GLsizei>(width),
        static_cast<GLsizei>(height)
    );


    // 禁止深度测试，启用模型透明混合
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);

    glEnable(GL_BLEND);


    // glBlendFunc(
    //     GL_SRC_ALPHA,
    //     GL_ONE_MINUS_SRC_ALPHA
    // );


    // ========================================================
    // 保持与窗口一致的投影比例
    // ========================================================

    CubismMatrix44 projection;


    projection.Scale(
        1.0f,
        static_cast<csmFloat32>(width) /
        static_cast<csmFloat32>(height)
    );


    projection.MultiplyByMatrix(
        _modelMatrix
    );


    _renderer->SetMvpMatrix(
        &projection
    );


    // ========================================================
    // 绘制模型
    // ========================================================

    _renderer->DrawModel();
}


// ============================================================
// 释放
// ============================================================

void Live2DModel::Shutdown()
{
    // ========================================================
    // 释放 Cubism Physics
    // ========================================================

    if (_physics)
    {
        CubismPhysics::Delete(
            _physics
        );

        _physics = nullptr;
    }


    // ========================================================
    // 释放 Mao 模型矩阵
    // ========================================================

    if (_modelMatrix)
    {
        delete _modelMatrix;

        _modelMatrix = nullptr;
    }


    // ========================================================
    // 释放 Mao 模型
    // ========================================================

    if (_model)
    {
        _model = nullptr;
    }


    if (_moc)
    {
        _moc = nullptr;
    }


    // ========================================================
    // 释放 model3.json 配置
    // ========================================================

    if (_setting)
    {
        delete _setting;

        _setting = nullptr;
    }


    // ========================================================
    // 关闭 Cubism Framework
    // ========================================================

    if (CubismFramework::IsInitialized())
    {
        CubismFramework::Dispose();
    }


    if (CubismFramework::IsStarted())
    {
        CubismFramework::CleanUp();
    }
}