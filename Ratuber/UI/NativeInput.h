#pragma once

#include <functional>
#include <memory>
#include <string>

// ============================================================
// NativeInput.h
//
// 职责：在主窗口里嵌一个 Windows 原生的单行输入框（EDIT 控件）。
//
// 为什么不用 SFML 自己画：SFML 窗口里中文输入法的候选词窗口出不来，
// 原生控件的中文输入、粘贴、选择、撤销都是系统自带的。
//
// 本头文件不包含 Windows.h。
//
// 线程：必须在创建窗口的线程（主线程）使用。
// 回车的回调是在 SFML 的 pollEvent() 里（处理窗口消息时）被调用的，
// 也在主线程。
// ============================================================

namespace Ratuber::UI
{
    class NativeInput
    {
    public:
        NativeInput();

        ~NativeInput();

        NativeInput(const NativeInput&) = delete;
        NativeInput& operator=(const NativeInput&) = delete;

        // parentWindow 是父窗口的 HWND（SFML 的 window.getNativeHandle()）。
        // x、y、width、height 是相对父窗口客户区的像素位置。
        // fontPixelHeight 是字体高度（像素）。
        bool Create(
            void* parentWindow,
            int x,
            int y,
            int width,
            int height,
            int fontPixelHeight
        );

        void Destroy();

        bool IsCreated() const;

        void SetBounds(int x, int y, int width, int height);

        void SetEnabled(bool enabled);

        void Focus();

        bool HasFocus() const;

        // 输入框里的文字（UTF-8）。
        std::string GetText() const;

        void Clear();

        // 用户按下回车时调用（输入法正在组字时的回车不算）。
        void SetOnEnter(std::function<void()> callback);

        // 内部实现（窗口过程需要访问，外部不要用）。
        struct Impl;

    private:
        std::unique_ptr<Impl> m_impl;
    };
}
