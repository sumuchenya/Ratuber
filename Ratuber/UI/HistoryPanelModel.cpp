#include "HistoryPanelModel.h"

#include <algorithm>

#include "BubbleModel.h"

namespace Ratuber::UI
{
    namespace
    {
        constexpr size_t kMaxNoteChars = 14;

        std::u32string ChipFor(const HistoryItem& item)
        {
            switch (item.status)
            {
            case ItemStatus::Waiting:   return U"排队中";
            case ItemStatus::Thinking:  return U"思考中…";
            case ItemStatus::Ready:     return U"等待播放";
            case ItemStatus::Speaking:  return U"播放中";
            case ItemStatus::Skipped:   return U"已跳过";
            case ItemStatus::Cancelled: return U"已取消";

            case ItemStatus::Failed:
            {
                std::u32string chip = U"失败";

                if (!item.note.empty())
                {
                    std::u32string note = Utf8ToUtf32(item.note);

                    if (note.size() > kMaxNoteChars)
                    {
                        note.resize(kMaxNoteChars);

                        note += U"…";
                    }

                    chip += U"：" + note;
                }

                return chip;
            }

            case ItemStatus::Done:
            default:
                return std::u32string();
            }
        }

        CardButton ButtonFor(ItemStatus status)
        {
            switch (status)
            {
            case ItemStatus::Waiting:
            case ItemStatus::Thinking:
            case ItemStatus::Ready:
                return CardButton::Cancel;

            case ItemStatus::Speaking:
                return CardButton::Skip;

            default:
                return CardButton::None;
            }
        }

        std::u32string OneLine(const std::string& utf8)
        {
            std::u32string text = Utf8ToUtf32(utf8);

            for (char32_t& c : text)
            {
                if (c == U'\n' || c == U'\r' || c == U'\t')
                {
                    c = U' ';
                }
            }

            return text;
        }
    }

    HistoryPanelModel::HistoryPanelModel(PanelMetrics metrics)
        :
        m_metrics(metrics),
        m_measure([](char32_t) { return 14.0f; })
    {}

    void HistoryPanelModel::SetMeasure(Measure measure)
    {
        m_measure = std::move(measure);

        Rebuild();
    }

    void HistoryPanelModel::SetViewportHeight(float height)
    {
        m_metrics.viewportHeight = std::max(0.0f, height);

        m_scroll = std::min(m_scroll, MaxScroll());
    }

    void HistoryPanelModel::SetItems(const std::vector<HistoryItem>& items)
    {
        if (items == m_items)
        {
            return;
        }

        const bool wasAtBottom = m_scroll >= MaxScroll() - 4.0f;

        m_items = items;

        Rebuild();

        if (wasAtBottom)
        {
            ScrollToBottom();
        }
        else
        {
            m_scroll = std::min(m_scroll, MaxScroll());
        }
    }

    void HistoryPanelModel::Rebuild()
    {
        m_cards.clear();

        m_queued = 0;

        const float wrapWidth =
            m_metrics.width - 2.0f * m_metrics.padding - 2.0f * m_metrics.cardPadding;

        float y = m_metrics.padding;

        for (const HistoryItem& item : m_items)
        {
            CardLayout card;

            card.id = item.id;
            card.status = item.status;
            card.chip = ChipFor(item);
            card.button = ButtonFor(item.status);
            card.hasHeader = card.button != CardButton::None || !card.chip.empty();

            if (card.button == CardButton::Cancel || item.status == ItemStatus::Thinking
                || item.status == ItemStatus::Waiting || item.status == ItemStatus::Ready)
            {
                ++m_queued;
            }

            card.userLines = WrapText(U"你：" + OneLine(item.user), wrapWidth, m_measure);

            if (!item.reply.empty())
            {
                card.replyLines = WrapText(U"AI：" + OneLine(item.reply), wrapWidth, m_measure);
            }

            float height = 2.0f * m_metrics.cardPadding;

            if (card.hasHeader)
            {
                height += m_metrics.headerHeight;
            }

            height += static_cast<float>(card.userLines.size()) * m_metrics.lineHeight;

            height += static_cast<float>(card.replyLines.size()) * m_metrics.lineHeight;

            card.top = y;
            card.height = height;

            if (card.button != CardButton::None)
            {
                card.buttonRect = PanelRect{
                    m_metrics.width - m_metrics.padding - m_metrics.cardPadding
                        - m_metrics.buttonWidth,
                    y + m_metrics.cardPadding
                        + (m_metrics.headerHeight - m_metrics.buttonHeight) / 2.0f,
                    m_metrics.buttonWidth,
                    m_metrics.buttonHeight
                };
            }

            y += height + m_metrics.cardGap;

            m_cards.push_back(std::move(card));
        }

        m_contentHeight = m_cards.empty()
            ? 2.0f * m_metrics.padding
            : y - m_metrics.cardGap + m_metrics.padding;

        m_scroll = std::min(m_scroll, MaxScroll());
    }

    // ============================================================
    // 展开 / 收起
    // ============================================================

    void HistoryPanelModel::Toggle()
    {
        m_open = !m_open;
    }

    void HistoryPanelModel::SetOpen(bool open)
    {
        m_open = open;
    }

    void HistoryPanelModel::Update(double deltaSeconds)
    {
        if (deltaSeconds <= 0.0 || m_metrics.slideSeconds <= 0.0f)
        {
            return;
        }

        const float step = static_cast<float>(deltaSeconds) / m_metrics.slideSeconds;

        if (m_open)
        {
            m_progress = std::min(1.0f, m_progress + step);
        }
        else
        {
            m_progress = std::max(0.0f, m_progress - step);
        }
    }

    float HistoryPanelModel::SlideOffset() const
    {
        // smoothstep：起步和停下都比较柔和。
        const float t = m_progress;

        const float eased = t * t * (3.0f - 2.0f * t);

        return m_metrics.width * (1.0f - eased);
    }


    // ============================================================
    // 滚动
    // ============================================================

    float HistoryPanelModel::MaxScroll() const
    {
        return std::max(0.0f, m_contentHeight - m_metrics.viewportHeight);
    }

    void HistoryPanelModel::ScrollBy(float delta)
    {
        m_scroll = std::clamp(m_scroll + delta, 0.0f, MaxScroll());
    }

    void HistoryPanelModel::ScrollToBottom()
    {
        m_scroll = MaxScroll();
    }


    // ============================================================
    // 点击
    // ============================================================

    HistoryPanelModel::HitResult HistoryPanelModel::HitTest(float x, float y) const
    {
        if (y < 0.0f || y >= m_metrics.viewportHeight)
        {
            return {};
        }

        const float contentY = y + m_scroll;

        for (const CardLayout& card : m_cards)
        {
            if (card.button != CardButton::None
                && card.buttonRect.Contains(x, contentY))
            {
                return { card.button, card.id };
            }
        }

        return {};
    }
}
