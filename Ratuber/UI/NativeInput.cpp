#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include "NativeInput.h"

#include <vector>

namespace Ratuber::UI
{
    struct NativeInput::Impl
    {
        HWND hwnd = nullptr;

        HFONT font = nullptr;

        WNDPROC originalProc = nullptr;

        std::function<void()> onEnter;
    };

    namespace
    {
        constexpr int kMaxChars = 500;

        // 子类化输入框：自己处理回车和 Esc。
        LRESULT CALLBACK EditProc(
            HWND hwnd,
            UINT message,
            WPARAM wParam,
            LPARAM lParam
        )
        {
            auto* impl = reinterpret_cast<NativeInput::Impl*>(
                GetWindowLongPtrW(hwnd, GWLP_USERDATA));

            if (impl == nullptr)
            {
                return DefWindowProcW(hwnd, message, wParam, lParam);
            }

            switch (message)
            {
            case WM_KEYDOWN:
                // 输入法组字时按键会变成 VK_PROCESSKEY，不会走到这里，
                // 所以输入法里按回车上屏不会被当成“发送”。
                if (wParam == VK_RETURN)
                {
                    if (impl->onEnter)
                    {
                        impl->onEnter();
                    }

                    return 0;
                }

                if (wParam == VK_ESCAPE)
                {
                    SetWindowTextW(hwnd, L"");

                    return 0;
                }

                break;

            case WM_CHAR:
                // 吞掉回车和 Esc 产生的字符，否则会“叮”一声。
                if (wParam == L'\r' || wParam == L'\n' || wParam == 27)
                {
                    return 0;
                }

                break;

            case WM_NCDESTROY:
            {
                WNDPROC original = impl->originalProc;

                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);

                SetWindowLongPtrW(
                    hwnd, GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(original));

                return CallWindowProcW(original, hwnd, message, wParam, lParam);
            }

            default:
                break;
            }

            return CallWindowProcW(
                impl->originalProc, hwnd, message, wParam, lParam);
        }
    }

    NativeInput::NativeInput()
        :
        m_impl(std::make_unique<Impl>())
    {}

    NativeInput::~NativeInput()
    {
        Destroy();
    }

    bool NativeInput::IsCreated() const
    {
        return m_impl->hwnd != nullptr;
    }

    bool NativeInput::Create(
        void* parentWindow,
        int x,
        int y,
        int width,
        int height,
        int fontPixelHeight
    )
    {
        Destroy();

        HWND parent = static_cast<HWND>(parentWindow);

        if (parent == nullptr)
        {
            return false;
        }

        // OpenGL 画的父窗口如果不裁剪子窗口，每一帧都会把输入框盖掉（闪烁）。
        LONG_PTR style = GetWindowLongPtrW(parent, GWL_STYLE);

        if ((style & WS_CLIPCHILDREN) == 0)
        {
            SetWindowLongPtrW(parent, GWL_STYLE, style | WS_CLIPCHILDREN);

            SetWindowPos(
                parent, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }

        HWND edit = CreateWindowExW(
            0,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL,
            x, y, width, height,
            parent,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr
        );

        if (edit == nullptr)
        {
            return false;
        }

        m_impl->hwnd = edit;

        m_impl->font = CreateFontW(
            -fontPixelHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            L"Microsoft YaHei UI");

        if (m_impl->font != nullptr)
        {
            SendMessageW(edit, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_impl->font), TRUE);
        }

        SendMessageW(edit, EM_SETLIMITTEXT, kMaxChars, 0);

        SendMessageW(edit, EM_SETMARGINS,
            EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(4, 4));

        SetWindowLongPtrW(
            edit, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(m_impl.get()));

        m_impl->originalProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(
                edit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&EditProc)));

        return true;
    }

    void NativeInput::Destroy()
    {
        if (m_impl->hwnd != nullptr)
        {
            DestroyWindow(m_impl->hwnd);

            m_impl->hwnd = nullptr;
        }

        if (m_impl->font != nullptr)
        {
            DeleteObject(m_impl->font);

            m_impl->font = nullptr;
        }
    }

    void NativeInput::SetBounds(int x, int y, int width, int height)
    {
        if (m_impl->hwnd != nullptr)
        {
            MoveWindow(m_impl->hwnd, x, y, width, height, TRUE);
        }
    }

    void NativeInput::SetEnabled(bool enabled)
    {
        if (m_impl->hwnd != nullptr)
        {
            EnableWindow(m_impl->hwnd, enabled ? TRUE : FALSE);
        }
    }

    void NativeInput::Focus()
    {
        if (m_impl->hwnd != nullptr && IsWindowEnabled(m_impl->hwnd))
        {
            SetFocus(m_impl->hwnd);
        }
    }

    bool NativeInput::HasFocus() const
    {
        return m_impl->hwnd != nullptr && GetFocus() == m_impl->hwnd;
    }

    std::string NativeInput::GetText() const
    {
        if (m_impl->hwnd == nullptr)
        {
            return std::string();
        }

        const int length = GetWindowTextLengthW(m_impl->hwnd);

        if (length <= 0)
        {
            return std::string();
        }

        std::vector<wchar_t> buffer(static_cast<size_t>(length) + 1);

        GetWindowTextW(m_impl->hwnd, buffer.data(), length + 1);

        const int bytes = WideCharToMultiByte(
            CP_UTF8, 0, buffer.data(), -1, nullptr, 0, nullptr, nullptr);

        if (bytes <= 1)
        {
            return std::string();
        }

        std::string utf8(static_cast<size_t>(bytes) - 1, '\0');

        WideCharToMultiByte(
            CP_UTF8, 0, buffer.data(), -1, utf8.data(), bytes, nullptr, nullptr);

        return utf8;
    }

    void NativeInput::Clear()
    {
        if (m_impl->hwnd != nullptr)
        {
            SetWindowTextW(m_impl->hwnd, L"");
        }
    }

    void NativeInput::SetOnEnter(std::function<void()> callback)
    {
        m_impl->onEnter = std::move(callback);
    }
}
