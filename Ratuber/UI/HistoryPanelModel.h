#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// ============================================================
// HistoryPanelModel.h
//
// 职责：右侧“对话”面板的数据和布局。只有逻辑，不依赖 SFML 和 Windows，
//       可以单独测试。
//
// 面板里是一列“卡片”，一张卡片 = 一个任务（用户的一句话和 AI 的回复），
// 按时间顺序从上到下排列：
//
//   ┌───────────────────────────┐
//   │ 排队中                [取消] │   <- 标题行（状态 + 按钮），已完成的没有
//   │ 你：今天天气怎么样          │
//   │ AI：今天天气不错呀……       │
//   └───────────────────────────┘
//
//   Waiting / Thinking / Ready  显示“取消”按钮
//   Speaking                    显示“跳过”按钮
//   Done                        没有标题行
//   Skipped / Cancelled / Failed  显示状态文字
//
// 新卡片出现时，如果用户本来就看着最底部，就自动滚到最底部；
// 用户往上翻了的话不打扰他。
// ============================================================

namespace Ratuber::UI
{
    enum class ItemStatus
    {
        Waiting,
        Thinking,
        Ready,
        Speaking,
        Done,
        Skipped,
        Cancelled,
        Failed,
    };

    struct HistoryItem
    {
        std::uint64_t id = 0;

        std::string user;

        std::string reply;

        ItemStatus status = ItemStatus::Waiting;

        // Failed 时的原因。
        std::string note;

        bool operator==(const HistoryItem& other) const
        {
            return id == other.id && user == other.user && reply == other.reply
                && status == other.status && note == other.note;
        }
    };

    struct PanelRect
    {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;

        bool Contains(float px, float py) const
        {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    enum class CardButton
    {
        None,
        Cancel,
        Skip,
    };

    struct CardLayout
    {
        std::uint64_t id = 0;

        ItemStatus status = ItemStatus::Done;

        // 在内容坐标里的位置（内容的最上方是 0）。
        float top = 0.0f;

        float height = 0.0f;

        // 标题行里的状态文字，没有则为空。
        std::u32string chip;

        CardButton button = CardButton::None;

        // 按钮位置：x 相对面板左边，y 是内容坐标。
        PanelRect buttonRect;

        // 是否有标题行。
        bool hasHeader = false;

        std::vector<std::u32string> userLines;

        std::vector<std::u32string> replyLines;
    };

    struct PanelMetrics
    {
        float width = 300.0f;

        // 面板内容区离面板边缘的距离。
        float padding = 12.0f;

        float cardPadding = 8.0f;

        float cardGap = 8.0f;

        float lineHeight = 22.0f;

        float headerHeight = 26.0f;

        float buttonWidth = 52.0f;

        float buttonHeight = 22.0f;

        // 内容区（可滚动的部分）的可见高度。
        float viewportHeight = 600.0f;

        // 展开 / 收起动画的时间（秒）。
        float slideSeconds = 0.18f;
    };

    class HistoryPanelModel
    {
    public:
        using Measure = std::function<float(char32_t)>;

        struct HitResult
        {
            CardButton kind = CardButton::None;

            std::uint64_t id = 0;
        };

        explicit HistoryPanelModel(PanelMetrics metrics = PanelMetrics());

        // 一个字符的像素宽度（折行用）。
        void SetMeasure(Measure measure);

        void SetViewportHeight(float height);

        void SetItems(const std::vector<HistoryItem>& items);

        const PanelMetrics& Metrics() const { return m_metrics; }

        const std::vector<CardLayout>& Cards() const { return m_cards; }

        // 还没轮到播放的任务数（Waiting / Thinking / Ready）。
        int QueuedCount() const { return m_queued; }

        // ---- 展开 / 收起 ----
        void Toggle();

        void SetOpen(bool open);

        bool IsOpen() const { return m_open; }

        void Update(double deltaSeconds);

        // 0 = 完全收起，1 = 完全展开。
        float Progress() const { return m_progress; }

        // 面板向右偏移了多少像素（完全展开 = 0，完全收起 = width）。
        float SlideOffset() const;

        // ---- 滚动 ----
        float ContentHeight() const { return m_contentHeight; }

        float MaxScroll() const;

        float ScrollY() const { return m_scroll; }

        void ScrollBy(float delta);

        void ScrollToBottom();

        // ---- 点击 ----
        // x、y 是相对内容区左上角的位置（y 不含滚动）。
        HitResult HitTest(float x, float y) const;

    private:
        void Rebuild();

        PanelMetrics m_metrics;

        Measure m_measure;

        std::vector<HistoryItem> m_items;

        std::vector<CardLayout> m_cards;

        float m_contentHeight = 0.0f;

        float m_scroll = 0.0f;

        int m_queued = 0;

        bool m_open = false;

        float m_progress = 0.0f;
    };
}
