cmake_minimum_required(VERSION 3.16)

# SFML 已经装在 vcpkg_installed 里，不让 vcpkg 每次配置时联网检查。
set(VCPKG_MANIFEST_INSTALL OFF CACHE BOOL "" FORCE)

project(Ratuber LANGUAGES CXX)

# ============================================================
# Ratuber + Live2D Cubism SDK
# ============================================================


# ============================================================
# Live2D SDK 路径
# ============================================================

# CMakeLists.txt 位于：
# D:/Ratuber/Ratuber/
#
# SDK 位于：
# D:/Ratuber/CubismSdkForNative-5-r.5/
#
# 所以需要返回上一层目录。

set(CUBISM_SDK_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../CubismSdkForNative-5-r.5"
)

set(CUBISM_CORE_PATH
    "${CUBISM_SDK_ROOT}/Core"
)

set(CUBISM_FRAMEWORK_PATH
    "${CUBISM_SDK_ROOT}/Framework"
)


# ============================================================
# Live2D Cubism Core
# ============================================================

add_library(Live2DCubismCore STATIC IMPORTED)

set_target_properties(
    Live2DCubismCore
    PROPERTIES
        IMPORTED_LOCATION_DEBUG
            "${CUBISM_CORE_PATH}/lib/windows/x86_64/143/Live2DCubismCore_MTd.lib"

        IMPORTED_LOCATION_RELEASE
            "${CUBISM_CORE_PATH}/lib/windows/x86_64/143/Live2DCubismCore_MT.lib"

        INTERFACE_INCLUDE_DIRECTORIES
            "${CUBISM_CORE_PATH}/include"
)


# ============================================================
# OpenGL
# ============================================================

find_package(OpenGL REQUIRED)


# ============================================================
# GLEW / GLFW 路径
# ============================================================

set(GLEW_PATH
    "${CUBISM_SDK_ROOT}/Samples/OpenGL/thirdParty/glew"
)

set(GLFW_PATH
    "${CUBISM_SDK_ROOT}/Samples/OpenGL/thirdParty/glfw"
)


# ============================================================
# GLEW / GLFW 配置
# ============================================================

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)


# ============================================================
# GLEW
# ============================================================

add_subdirectory(
    "${GLEW_PATH}/build/cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/glew"
)


# ============================================================
# Live2D Framework
# ============================================================

# 使用 OpenGL Renderer
set(FRAMEWORK_SOURCE OpenGL)

# OpenGL Framework 头文件
set(RENDER_INCLUDE_PATH
    "${CUBISM_FRAMEWORK_PATH}/src/Rendering/OpenGL"
)

add_subdirectory(
    "${CUBISM_FRAMEWORK_PATH}"
    "${CMAKE_CURRENT_BINARY_DIR}/Framework"
)


# ============================================================
# Framework 编译配置
# ============================================================

target_compile_definitions(
    Framework
    PUBLIC
        CSM_TARGET_WIN_GL
)


# ============================================================
# Framework 头文件
# ============================================================

target_include_directories(
    Framework
    PUBLIC
        "${GLEW_PATH}/include"
)


# ============================================================
# Framework 依赖
# ============================================================

target_link_libraries(
    Framework
    PUBLIC
        Live2DCubismCore
        glew_s
)


# ============================================================
# SFML
# ============================================================

set(SFML_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-windows/share/sfml"
)

find_package(
    SFML
    COMPONENTS
        Graphics
        Window
        System
    CONFIG
    REQUIRED
)


# ============================================================
# 【改】nlohmann/json（AI 层解析 JSON，只有头文件）
# ============================================================

# 先找 vcpkg 安装的，再找项目里 third_party 目录自带的。
find_path(
    NLOHMANN_JSON_INCLUDE_DIR
    nlohmann/json.hpp
    HINTS
        "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-windows/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
)

if(NOT NLOHMANN_JSON_INCLUDE_DIR)
    message(FATAL_ERROR
        "找不到 nlohmann/json.hpp，请先 vcpkg install nlohmann-json:x64-windows")
endif()


# ============================================================
# 自动收集行为系统源文件
# ============================================================

# 自动收集 behavior 目录中的所有 .cpp。
# CONFIGURE_DEPENDS 会让 CMake 在重新构建时检查目录
# 是否新增或删除了源文件。

file(
    GLOB_RECURSE
    RATUBER_BEHAVIOR_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/behavior/*.cpp"
)


# ============================================================
# 【改】Ratuber 主程序
# ============================================================

add_executable(
    Ratuber

    # 主程序
    "main.cpp"

    # Live2D 模型
    "Live2DModel/Live2DModel.cpp"

    # Motion
    "motion/MotionManager.cpp"

    # Expression
    "expression/ExpressionManager.cpp"

    # Blink
    "blink/BlinkManager.cpp"

    # Mouth Open
    "MouthOpen/MouthOpenManager.cpp"

    # AI 层（文字聊天 + 语音合成 + 动作表情播放）
    "AI/AIManager.cpp"
    "AI/AIProtocol.cpp"
    "AI/AIPlatform.cpp"
    "AI/API/DeepSeekChatAPI.cpp"
    "AI/NOWICANLISTEN/NowICanListen.cpp"
    "AI/NOWICANTALK/NowICanTalk.cpp"
    "AI/NOWICANPLAY/NowICanPlay.cpp"

    # 聊天界面
    "UI/BubbleModel.cpp"
    "UI/NativeInput.cpp"
    "UI/HistoryPanelModel.cpp"
    "UI/ChatUI.cpp"

    # 所有行为源文件
    ${RATUBER_BEHAVIOR_SOURCES}
)


# ============================================================
# Ratuber 链接库
# ============================================================

target_link_libraries(
    Ratuber
    PRIVATE

        # SFML
        SFML::Graphics
        SFML::Window
        SFML::System

        # Live2D Framework
        Framework

        # OpenGL
        OpenGL::GL

        # Windows Audio / HTTP
        winmm
        winhttp
)


# ============================================================
# 【改】Ratuber 头文件搜索路径
# ============================================================

target_include_directories(
    Ratuber
    PRIVATE

        # 项目根目录
        "${CMAKE_CURRENT_SOURCE_DIR}"

        # Cubism Framework
        "${CUBISM_FRAMEWORK_PATH}/src"
        "${CUBISM_FRAMEWORK_PATH}/src/Type"
        "${CUBISM_FRAMEWORK_PATH}/src/Model"
        "${CUBISM_FRAMEWORK_PATH}/src/Motion"
        "${CUBISM_FRAMEWORK_PATH}/src/Physics"
        "${CUBISM_FRAMEWORK_PATH}/src/Effect"
        "${CUBISM_FRAMEWORK_PATH}/src/Rendering"

        # GLEW
        "${GLEW_PATH}/include"

        # nlohmann/json
        "${NLOHMANN_JSON_INCLUDE_DIR}"
)


# ============================================================
# C++ 标准
# ============================================================

set_property(
    TARGET Ratuber
    PROPERTY CXX_STANDARD 20
)

set_property(
    TARGET Ratuber
    PROPERTY CXX_STANDARD_REQUIRED ON
)


# ============================================================
# 【改】编译宏
# ============================================================

# RATUBER_MAO_ASSET_DIR：Mao 模型资源目录
# RATUBER_PROJECT_DIR  ：AIManager 用它找 AI/Server 和 tempvoice

target_compile_definitions(
    Ratuber
    PRIVATE
        RATUBER_MAO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/Mao"
        RATUBER_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        NOMINMAX
)


# ============================================================
# MSVC 编译配置
# ============================================================

if(MSVC)
    target_compile_options(
        Ratuber
        PRIVATE
            /utf-8
    )
endif()


# ============================================================
# 【改】测试 1：面板逻辑（不需要 SFML，先跑这个）
# ============================================================

add_executable(
    HistoryPanelModelTest
    "UI/Tests/HistoryPanelModelTest.cpp"
    "UI/HistoryPanelModel.cpp"
    "UI/BubbleModel.cpp"
)

target_include_directories(
    HistoryPanelModelTest
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

set_property(TARGET HistoryPanelModelTest PROPERTY CXX_STANDARD 20)
set_property(TARGET HistoryPanelModelTest PROPERTY CXX_STANDARD_REQUIRED ON)


# ============================================================
# 【改】测试 2：聊天界面（不需要 Live2D 和 AI 服务器）
#   运行：ChatUISmokeTest        普通窗口
#         ChatUISmokeTest pet    无边框置顶透明窗口
# ============================================================

add_executable(
    ChatUISmokeTest
    "UI/Tests/ChatUISmokeTest.cpp"
    "UI/BubbleModel.cpp"
    "UI/NativeInput.cpp"
    "UI/HistoryPanelModel.cpp"
    "UI/ChatUI.cpp"
)

target_include_directories(
    ChatUISmokeTest
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(
    ChatUISmokeTest
    PRIVATE
        SFML::Graphics
        SFML::Window
        SFML::System
)
target_compile_definitions(ChatUISmokeTest PRIVATE NOMINMAX)
set_property(TARGET ChatUISmokeTest PROPERTY CXX_STANDARD 20)
set_property(TARGET ChatUISmokeTest PROPERTY CXX_STANDARD_REQUIRED ON)

if(MSVC)
    target_compile_options(HistoryPanelModelTest PRIVATE /utf-8)
    target_compile_options(ChatUISmokeTest PRIVATE /utf-8)
endif()