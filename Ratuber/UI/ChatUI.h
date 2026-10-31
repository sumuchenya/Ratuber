#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <SFML/Graphics.hpp>

#include "BubbleModel.h"
#include "HistoryPanelModel.h"
#include "NativeInput.h"

// ============================================================
// ChatUI.h
//
// 职责：桌宠窗口里的文字聊天界面。
//
//   ┌──────────────────────────────┐
//   │         ┌──────────┐         │
//   │         │ AI 的回复 │ [跳过]   │  气泡（文字随声音逐字显现）
//   │         └────▽─────┘         │
//   │                              │
//   │        [  Live2D 角色  ]      │
//   │                              │
//   │  [提示文字]                    │  小提示（启动中 / 出错）
//   │ ┌──────────────────┐ ┌────┐ │
//   │ │ 输入框            │ │发送 │ │  输入框（原生控件，中文输入正常）
//   │ └──────────────────┘ └────┘ │
//   └──────────────────────────────┘
//
// 窗口是用“洋红色当透明色”的无边框窗口，所以所有东西都画成不透明的，
// 并且不开抗锯齿（抗锯齿会在边缘混出粉边）。
//
// 本类不认识 AIManager。main.cpp 把 AIManager 的事件转给下面三个 On...
// 函数，把用户的操作通过 SetOnSubmit / SetOnSkip 拿回去。
//
// ------------------------------------------------------------
// main.cpp 里的位置（每帧）：
//
//   事件循环里：  if (ui.HandleEvent(*event)) continue;
//                 拖动窗口前：  if (ui.IsPointOnUI(mouse->position)) 不拖动
//   每帧：        ui.Update(dt);
//   mao.Render() 之后、window.display() 之前：  ui.Draw(window);
// ------------------------------------------------------------
// ============================================================

namespace Ratuber::UI
{
    struct ChatUIConfig
    {
        // 字体文件。为空时依次找 微软雅黑、黑体、宋体。
        std::string fontPath;

        unsigned textSize = 20;

        unsigned statusTextSize = 14;

        float margin = 16.0f;

        float barHeight = 48.0f;

        float sendWidth = 72.0f;

        // 气泡：最大宽度、水平中心（小于 0 表示窗口正中）、顶部位置。
        float bubbleMaxWidth = 460.0f;

        float bubbleCenterX = -1.0f;

        float bubbleTop = 40.0f;

        // true：气泡固定在窗口右上角（不跟角色头顶走，也不画指向角色的小尾巴），
        // 不会挡住角色。顶边用 bubbleTop，右边留 bubbleRightMargin 的空隙。
        bool bubbleTopRight = true;

        float bubbleRightMargin = 16.0f;

        // 气泡底边的位置（窗口像素，从上往下数）。大于 0 时气泡的底边固定在这里，
        // 文字多了往上长；小于 0 时用 bubbleTop（顶边固定，往下长）。
        // 调到角色头顶附近最好看。
        float bubbleBottom = -1.0f;

        // 右侧“对话”标签的上边位置（窗口像素）。小于 0 时放在窗口高度的 42% 处。
        float historyTabTop = -1.0f;

        // 右侧面板展开后的上边位置（窗口像素）。大于等于 0 时面板从这里往下铺到输入框上方，
        // 用来让面板顶部只比角色头顶高一点点；小于 0 时从窗口最上面开始。
        float historyPanelTop = -1.0f;

        // 用 SetHeadTop() 跟随角色头顶时，各部分相对头顶的距离（像素）：
        //   气泡尾巴尖离头顶的空隙
        float headBubbleGap = 14.0f;
        //   展开的面板顶边比头顶高多少
        float headPanelRise = 30.0f;
        //   “对话”标签的上边比头顶低多少
        float headTabDrop = 70.0f;
    };

    class ChatUI
    {
    public:
        ChatUI();

        ~ChatUI();

        ChatUI(const ChatUI&) = delete;
        ChatUI& operator=(const ChatUI&) = delete;

        // 创建输入框、加载字体。
        // 返回 false 表示找不到中文字体（输入框仍然可用，但气泡里没有字）。
        bool Initialize(
            sf::RenderWindow& window,
            ChatUIConfig config = ChatUIConfig()
        );

        void Shutdown();

        // 用户按回车或点“发送”：参数是输入的文字（UTF-8，已去掉首尾空白）。
        void SetOnSubmit(std::function<void(const std::string&)> callback);

        // 用户点气泡上的“跳过”。
        void SetOnSkip(std::function<void()> callback);

        // 用户点面板里某个排队任务的“取消”（参数是任务编号）。
        void SetOnCancel(std::function<void(std::uint64_t)> callback);

        // 刷新右侧面板里的任务列表（每帧调用也可以，内容没变时不会重新排版）。
        void SetHistory(const std::vector<HistoryItem>& items);

        // 打开 / 收起右侧面板。
        void ToggleHistory();

        bool IsHistoryOpen() const;

        // 服务器没就绪时禁用输入。
        void SetInputEnabled(bool enabled);

        // 输入框上方的小提示。空字符串表示不显示。
        void SetStatus(const std::string& utf8, bool isError = false);

        void FocusInput();

        // 告诉界面角色头顶现在在窗口里的哪个高度（像素，从上往下数）。
        // 气泡、“对话”标签和展开的面板会跟着它走。模型缩放之后要重新调用。
        // headCenterX 是头顶的水平中心（窗口像素），气泡会对准它；小于 0 时气泡居中。
        void SetHeadTop(float headTopY, float headCenterX = -1.0f);

        // ---- 来自 AIManager 的播放事件（文字都是 UTF-8）----
        void OnSpeechStarted(const std::string& fullText);

        void OnSegmentStarted(const std::string& segmentText);

        void OnSpeechFinished(const std::string& fullText, bool skipped);

        // ---- 每帧 ----

        // 返回 true 表示这个事件被界面用掉了（点了按钮），不要再当作拖动窗口。
        bool HandleEvent(const sf::Event& event);

        // 这个点（窗口内像素坐标）是不是在输入框、按钮、提示上。
        bool IsPointOnUI(sf::Vector2i point) const;

        void Update(float deltaSeconds);

        // 在 Live2D 画完之后调用。内部会保存和恢复 OpenGL 状态。
        void Draw(sf::RenderWindow& window);

    private:
        float MeasureChar(char32_t c, unsigned size);

        void RebuildBubbleLayout();

        void SubmitFromInput();

        void UpdateStatusRect();

        float MeasurePanelChar(char32_t c);

        sf::FloatRect PanelRect() const;

        sf::FloatRect TabRect() const;

        sf::FloatRect PanelContentRect() const;

        void DrawHistoryPanel(sf::RenderWindow& window);

        ChatUIConfig m_config;

        sf::Font m_font;

        bool m_fontLoaded = false;

        sf::Vector2u m_windowSize{ 800, 1000 };

        BubbleModel m_bubble;

        NativeInput m_input;

        bool m_inputEnabled = true;

        std::function<void(const std::string&)> m_onSubmit;

        std::function<void()> m_onSkip;

        // ---- 布局 ----
        sf::FloatRect m_barRect;
        sf::FloatRect m_sendRect;
        sf::FloatRect m_skipRect;
        sf::FloatRect m_statusRect;
        sf::FloatRect m_bubbleRect;

        // 气泡文字的折行结果（完整回复）。
        std::vector<std::u32string> m_lines;

        std::u32string m_layoutText;

        float m_lineHeight = 24.0f;

        std::string m_status;

        bool m_statusIsError = false;

        bool m_hoverSend = false;

        bool m_hoverSkip = false;

        bool m_pressConsumed = false;

        std::unordered_map<char32_t, float> m_charWidth;

        // ---- 右侧面板 ----
        HistoryPanelModel m_panel;

        std::function<void(std::uint64_t)> m_onCancel;

        std::unordered_map<char32_t, float> m_panelCharWidth;

        bool m_hoverTab = false;

        std::uint64_t m_hoverButtonId = 0;
    };
}
