#include "ChatUI.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numbers>

namespace Ratuber::UI
{
    namespace
    {
        // ---- 颜色（全部不透明，也不要用洋红色，洋红色是透明色）----
        const sf::Color kBubbleFill(255, 251, 246);
        const sf::Color kBubbleLine(130, 100, 120);
        const sf::Color kTextColor(60, 42, 60);

        const sf::Color kBarFill(250, 247, 255);
        const sf::Color kBarLine(150, 130, 175);

        const sf::Color kButtonFill(150, 110, 200);
        const sf::Color kButtonHover(170, 130, 220);
        const sf::Color kButtonOff(185, 180, 195);
        const sf::Color kButtonText(255, 255, 255);

        const sf::Color kSkipFill(255, 238, 242);
        const sf::Color kSkipHover(255, 218, 228);
        const sf::Color kSkipLine(200, 120, 140);
        const sf::Color kSkipText(150, 60, 85);

        const sf::Color kStatusFill(240, 240, 248);
        const sf::Color kStatusText(90, 90, 110);
        const sf::Color kErrorFill(255, 232, 232);
        const sf::Color kErrorText(170, 40, 40);

        constexpr float kBubblePadding = 16.0f;
        constexpr float kTailHeight = 12.0f;
        constexpr float kTailHalfWidth = 10.0f;
        constexpr float kSkipWidth = 56.0f;
        constexpr float kSkipHeight = 26.0f;
        constexpr float kStatusHeight = 24.0f;

        // ---- 右侧面板 ----
        const sf::Color kPanelFill(246, 243, 252);
        const sf::Color kPanelLine(150, 130, 175);
        const sf::Color kPanelTitle(90, 60, 130);
        const sf::Color kCardFill(255, 255, 255);
        const sf::Color kCardLine(205, 195, 225);
        const sf::Color kCardActiveLine(150, 110, 200);
        const sf::Color kCardDimText(140, 135, 155);
        const sf::Color kChipText(110, 90, 150);
        const sf::Color kChipBad(170, 40, 40);
        const sf::Color kBadgeFill(220, 70, 100);

        constexpr float kPanelWidth = 300.0f;
        constexpr float kPanelTitleHeight = 38.0f;
        constexpr float kTabWidth = 28.0f;
        constexpr float kTabHeight = 84.0f;
        constexpr unsigned kPanelTextSize = 16;

        // 圆角矩形（不开抗锯齿，边缘是硬的，不会在透明色上混出粉边）。
        sf::ConvexShape MakeRoundedRect(
            const sf::FloatRect& rect,
            float radius,
            const sf::Color& fill,
            const sf::Color& line,
            float lineThickness
        )
        {
            constexpr int kSegments = 6;

            radius = std::min(radius, std::min(rect.size.x, rect.size.y) / 2.0f);

            sf::ConvexShape shape;

            shape.setPointCount(4 * (kSegments + 1));

            // 四个角：圆心、起始角度。
            const sf::Vector2f centers[4] =
            {
                { rect.position.x + rect.size.x - radius, rect.position.y + radius },
                { rect.position.x + rect.size.x - radius, rect.position.y + rect.size.y - radius },
                { rect.position.x + radius, rect.position.y + rect.size.y - radius },
                { rect.position.x + radius, rect.position.y + radius },
            };

            const float startDegrees[4] = { -90.0f, 0.0f, 90.0f, 180.0f };

            std::size_t index = 0;

            for (int corner = 0; corner < 4; ++corner)
            {
                for (int i = 0; i <= kSegments; ++i)
                {
                    const float degrees =
                        startDegrees[corner] + 90.0f * static_cast<float>(i) / kSegments;

                    const float radians = degrees * std::numbers::pi_v<float> / 180.0f;

                    shape.setPoint(
                        index++,
                        {
                            centers[corner].x + radius * std::cos(radians),
                            centers[corner].y + radius * std::sin(radians)
                        });
                }
            }

            shape.setFillColor(fill);
            shape.setOutlineColor(line);
            shape.setOutlineThickness(lineThickness);

            return shape;
        }

        std::string TrimSpaces(const std::string& text)
        {
            size_t begin = 0;
            size_t end = text.size();

            while (begin < end
                && (text[begin] == ' ' || text[begin] == '\t'
                    || text[begin] == '\r' || text[begin] == '\n'))
            {
                ++begin;
            }

            while (end > begin
                && (text[end - 1] == ' ' || text[end - 1] == '\t'
                    || text[end - 1] == '\r' || text[end - 1] == '\n'))
            {
                --end;
            }

            return text.substr(begin, end - begin);
        }

        // 换行符统一换成空格：这样“第几个字”和折行之后的位置一一对应。
        std::u32string Normalize(std::u32string text)
        {
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

    ChatUI::ChatUI() = default;

    ChatUI::~ChatUI()
    {
        Shutdown();
    }

    // ============================================================
    // 初始化
    // ============================================================

    bool ChatUI::Initialize(
        sf::RenderWindow& window,
        ChatUIConfig config
    )
    {
        m_config = std::move(config);

        m_windowSize = window.getSize();

        const float width = static_cast<float>(m_windowSize.x);
        const float height = static_cast<float>(m_windowSize.y);

        // ---- 布局 ----

        const float margin = m_config.margin;

        m_barRect = sf::FloatRect(
            { margin, height - margin - m_config.barHeight },
            { width - margin * 2.0f - m_config.sendWidth - 8.0f, m_config.barHeight }
        );

        m_sendRect = sf::FloatRect(
            { width - margin - m_config.sendWidth, m_barRect.position.y },
            { m_config.sendWidth, m_config.barHeight }
        );

        // ---- 字体 ----

        std::vector<std::string> candidates;

        if (!m_config.fontPath.empty())
        {
            candidates.push_back(m_config.fontPath);
        }

        const char* windir = std::getenv("WINDIR");

        const std::string fonts =
            std::string(windir != nullptr ? windir : "C:\\Windows") + "\\Fonts\\";

        candidates.push_back(fonts + "msyh.ttc");
        candidates.push_back(fonts + "msyh.ttf");
        candidates.push_back(fonts + "simhei.ttf");
        candidates.push_back(fonts + "simsun.ttc");
        candidates.push_back(fonts + "Deng.ttf");

        m_fontLoaded = false;

        for (const std::string& path : candidates)
        {
            if (m_font.openFromFile(path))
            {
                m_fontLoaded = true;

                break;
            }
        }

        m_lineHeight = m_fontLoaded
            ? m_font.getLineSpacing(m_config.textSize) + 4.0f
            : static_cast<float>(m_config.textSize) + 6.0f;

        // ---- 输入框 ----

        const int fontPixels = static_cast<int>(m_config.textSize);

        const int editHeight = fontPixels + 8;

        const bool created = m_input.Create(
            // SFML 的 getNativeHandle 在 Windows 上就是 HWND。
            (void*)window.getNativeHandle(),
            static_cast<int>(m_barRect.position.x + 10.0f),
            static_cast<int>(m_barRect.position.y
                + (m_barRect.size.y - static_cast<float>(editHeight)) / 2.0f),
            static_cast<int>(m_barRect.size.x - 20.0f),
            editHeight,
            fontPixels
        );

        m_input.SetOnEnter([this] { SubmitFromInput(); });

        // ---- 右侧面板 ----
        {
            PanelMetrics metrics;

            metrics.width = std::min(kPanelWidth, width - 40.0f);

            metrics.lineHeight = m_fontLoaded
                ? m_font.getLineSpacing(kPanelTextSize) + 2.0f
                : 22.0f;

            metrics.viewportHeight = PanelContentRect().size.y;

            m_panel = HistoryPanelModel(metrics);

            m_panel.SetMeasure([this](char32_t c) { return MeasurePanelChar(c); });
        }

        UpdateStatusRect();

        return created && m_fontLoaded;
    }

    void ChatUI::Shutdown()
    {
        m_input.Destroy();
    }

    void ChatUI::SetOnSubmit(std::function<void(const std::string&)> callback)
    {
        m_onSubmit = std::move(callback);
    }

    void ChatUI::SetOnSkip(std::function<void()> callback)
    {
        m_onSkip = std::move(callback);
    }

    void ChatUI::SetOnCancel(std::function<void(std::uint64_t)> callback)
    {
        m_onCancel = std::move(callback);
    }

    void ChatUI::SetHistory(const std::vector<HistoryItem>& items)
    {
        m_panel.SetItems(items);
    }

    void ChatUI::ToggleHistory()
    {
        m_panel.Toggle();
    }

    bool ChatUI::IsHistoryOpen() const
    {
        return m_panel.IsOpen();
    }

    float ChatUI::MeasurePanelChar(char32_t c)
    {
        if (!m_fontLoaded)
        {
            return static_cast<float>(kPanelTextSize);
        }

        const auto it = m_panelCharWidth.find(c);

        if (it != m_panelCharWidth.end())
        {
            return it->second;
        }

        const float advance = m_font.getGlyph(c, kPanelTextSize, false).advance;

        m_panelCharWidth[c] = advance;

        return advance;
    }

    // 面板：贴着窗口右边，上下范围是从窗口顶到输入框上方（不盖住输入框和发送按钮）。
    sf::FloatRect ChatUI::PanelRect() const
    {
        const float width = m_panel.Metrics().width;

        const float top = m_config.historyPanelTop >= 0.0f
            ? m_config.historyPanelTop
            : 0.0f;

        const float height = std::max(100.0f, m_barRect.position.y - 8.0f - top);

        return sf::FloatRect(
            { static_cast<float>(m_windowSize.x) - width + m_panel.SlideOffset(), top },
            { width, height });
    }

    sf::FloatRect ChatUI::TabRect() const
    {
        const sf::FloatRect panel = PanelRect();

        const float top = m_config.historyTabTop >= 0.0f
            ? m_config.historyTabTop
            : static_cast<float>(m_windowSize.y) * 0.42f;

        return sf::FloatRect(
            { panel.position.x - kTabWidth, top },
            { kTabWidth, kTabHeight });
    }

    // 内容区（卡片滚动的区域），窗口坐标。
    sf::FloatRect ChatUI::PanelContentRect() const
    {
        const sf::FloatRect panel = PanelRect();

        return sf::FloatRect(
            { panel.position.x, panel.position.y + kPanelTitleHeight },
            { panel.size.x, panel.size.y - kPanelTitleHeight - 6.0f });
    }

    void ChatUI::SetInputEnabled(bool enabled)
    {
        m_inputEnabled = enabled;

        m_input.SetEnabled(enabled);
    }

    void ChatUI::SetHeadTop(float headTopY, float headCenterX)
    {
        // 尾巴尖 = 气泡底边 + 尾巴高度，所以底边要再往上让出一个尾巴。
        m_config.bubbleBottom = std::max(
            60.0f,
            headTopY - m_config.headBubbleGap - kTailHeight);

        if (!m_config.bubbleTopRight)
        {
            m_config.bubbleCenterX = headCenterX;
        }

        m_config.historyPanelTop = std::max(0.0f, headTopY - m_config.headPanelRise);

        m_config.historyTabTop = std::max(0.0f, headTopY + m_config.headTabDrop);

        // 让气泡按新的位置重新排一次。
        m_layoutText.clear();

        if (m_bubble.Visible())
        {
            RebuildBubbleLayout();
        }
    }

    void ChatUI::FocusInput()
    {
        m_input.Focus();
    }

    void ChatUI::SetStatus(const std::string& utf8, bool isError)
    {
        m_status = utf8;

        m_statusIsError = isError;

        UpdateStatusRect();
    }

    void ChatUI::UpdateStatusRect()
    {
        if (m_status.empty() || !m_fontLoaded)
        {
            m_statusRect = sf::FloatRect();

            return;
        }

        float width = 0.0f;

        for (const char32_t c : Utf8ToUtf32(m_status))
        {
            width += m_font.getGlyph(c, m_config.statusTextSize, false).advance;
        }

        width = std::min(
            width + 24.0f,
            static_cast<float>(m_windowSize.x) - m_config.margin * 2.0f);

        m_statusRect = sf::FloatRect(
            { m_config.margin, m_barRect.position.y - 8.0f - kStatusHeight },
            { width, kStatusHeight }
        );
    }


    // ============================================================
    // 事件（来自 AIManager）
    // ============================================================

    float ChatUI::MeasureChar(char32_t c, unsigned size)
    {
        if (!m_fontLoaded)
        {
            return static_cast<float>(size);
        }

        const auto it = m_charWidth.find(c);

        if (it != m_charWidth.end())
        {
            return it->second;
        }

        const float advance = m_font.getGlyph(c, size, false).advance;

        m_charWidth[c] = advance;

        return advance;
    }

    void ChatUI::RebuildBubbleLayout()
    {
        const std::u32string& text = m_bubble.FullText();

        if (text == m_layoutText && !m_lines.empty())
        {
            return;
        }

        m_layoutText = text;

        const unsigned size = m_config.textSize;

        m_lines = WrapText(
            text,
            m_config.bubbleMaxWidth - kBubblePadding * 2.0f,
            [this, size](char32_t c) { return MeasureChar(c, size); }
        );

        float widest = 0.0f;

        for (const std::u32string& line : m_lines)
        {
            float width = 0.0f;

            for (const char32_t c : line)
            {
                width += MeasureChar(c, size);
            }

            widest = std::max(widest, width);
        }

        const float bubbleWidth = std::clamp(
            widest + kBubblePadding * 2.0f + 2.0f,
            120.0f,
            m_config.bubbleMaxWidth + 2.0f);

        const float bubbleHeight =
            static_cast<float>(m_lines.size()) * m_lineHeight + kBubblePadding * 2.0f;

        const float windowWidth = static_cast<float>(m_windowSize.x);

        const float centerX = m_config.bubbleCenterX >= 0.0f
            ? m_config.bubbleCenterX
            : windowWidth / 2.0f;

        const float left = std::clamp(
            centerX - bubbleWidth / 2.0f,
            8.0f,
            std::max(8.0f, windowWidth - bubbleWidth - 8.0f));

        // 角落模式：贴着窗口右上角，顶边固定，文字多了往下长。
        // 否则底边固定：文字多了往上长，尾巴始终指在同一个位置（角色头顶）。
        float top = m_config.bubbleBottom > 0.0f
            ? std::max(8.0f, m_config.bubbleBottom - bubbleHeight)
            : m_config.bubbleTop;

        float bubbleLeft = left;

        if (m_config.bubbleTopRight)
        {
            top = m_config.bubbleTop;

            bubbleLeft = std::max(
                8.0f,
                windowWidth - m_config.bubbleRightMargin - bubbleWidth);
        }

        m_bubbleRect = sf::FloatRect(
            { bubbleLeft, top },
            { bubbleWidth, bubbleHeight }
        );

        m_skipRect = sf::FloatRect(
            {
                m_bubbleRect.position.x + m_bubbleRect.size.x - kSkipWidth,
                m_bubbleRect.position.y + m_bubbleRect.size.y
                    + (m_config.bubbleTopRight ? 0.0f : kTailHeight) + 6.0f
            },
            { kSkipWidth, kSkipHeight }
        );
    }

    void ChatUI::OnSpeechStarted(const std::string& fullText)
    {
        m_layoutText.clear();

        m_lines.clear();

        m_bubble.Start(Normalize(Utf8ToUtf32(fullText)));

        RebuildBubbleLayout();
    }

    void ChatUI::OnSegmentStarted(const std::string& segmentText)
    {
        m_bubble.AddSegment(Normalize(Utf8ToUtf32(segmentText)));
    }

    void ChatUI::OnSpeechFinished(const std::string& fullText, bool skipped)
    {
        m_bubble.Finish(Normalize(Utf8ToUtf32(fullText)), skipped);

        RebuildBubbleLayout();
    }


    // ============================================================
    // 输入
    // ============================================================

    void ChatUI::SubmitFromInput()
    {
        if (!m_inputEnabled)
        {
            return;
        }

        const std::string text = TrimSpaces(m_input.GetText());

        if (text.empty())
        {
            return;
        }

        m_input.Clear();

        m_input.Focus();

        if (m_onSubmit)
        {
            m_onSubmit(text);
        }
    }

    bool ChatUI::HandleEvent(const sf::Event& event)
    {
        if (const auto* moved = event.getIf<sf::Event::MouseMoved>())
        {
            const sf::Vector2f point(moved->position);

            m_hoverSend = m_sendRect.contains(point);

            m_hoverSkip = m_bubble.CanSkip() && m_skipRect.contains(point);

            m_hoverTab = TabRect().contains(point);

            m_hoverButtonId = 0;

            if (m_panel.Progress() > 0.0f)
            {
                const sf::FloatRect content = PanelContentRect();

                if (content.contains(point))
                {
                    m_hoverButtonId = m_panel.HitTest(
                        point.x - content.position.x,
                        point.y - content.position.y).id;
                }
            }

            return false;
        }

        if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>())
        {
            if (m_panel.Progress() > 0.0f
                && PanelRect().contains(sf::Vector2f(wheel->position)))
            {
                // 滚轮往上 = 看更早的内容。
                m_panel.ScrollBy(-wheel->delta * 48.0f);

                return true;
            }

            return false;
        }

        if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
        {
            if (pressed->button != sf::Mouse::Button::Left)
            {
                return false;
            }

            const sf::Vector2f point(pressed->position);

            if (TabRect().contains(point))
            {
                m_panel.Toggle();

                m_pressConsumed = true;

                return true;
            }

            if (m_panel.Progress() > 0.0f && PanelRect().contains(point))
            {
                const sf::FloatRect content = PanelContentRect();

                if (content.contains(point))
                {
                    const auto hit = m_panel.HitTest(
                        point.x - content.position.x,
                        point.y - content.position.y);

                    if (hit.kind == CardButton::Cancel && m_onCancel)
                    {
                        m_onCancel(hit.id);
                    }
                    else if (hit.kind == CardButton::Skip && m_onSkip)
                    {
                        m_onSkip();
                    }
                }

                m_pressConsumed = true;

                return true;
            }

            if (m_sendRect.contains(point))
            {
                SubmitFromInput();

                m_pressConsumed = true;

                return true;
            }

            if (m_bubble.CanSkip() && m_skipRect.contains(point))
            {
                if (m_onSkip)
                {
                    m_onSkip();
                }

                m_pressConsumed = true;

                return true;
            }

            // 点在输入框的底板上（输入框控件周围的一圈）：把焦点交给输入框。
            if (m_barRect.contains(point))
            {
                m_input.Focus();

                m_pressConsumed = true;

                return true;
            }

            return false;
        }

        if (event.is<sf::Event::MouseButtonReleased>())
        {
            if (m_pressConsumed)
            {
                m_pressConsumed = false;

                return true;
            }
        }

        return false;
    }

    bool ChatUI::IsPointOnUI(sf::Vector2i point) const
    {
        const sf::Vector2f p(point);

        if (m_barRect.contains(p) || m_sendRect.contains(p))
        {
            return true;
        }

        if (!m_status.empty() && m_statusRect.contains(p))
        {
            return true;
        }

        if (m_bubble.CanSkip() && m_skipRect.contains(p))
        {
            return true;
        }

        if (TabRect().contains(p))
        {
            return true;
        }

        if (m_panel.Progress() > 0.0f && PanelRect().contains(p))
        {
            return true;
        }

        return false;
    }

    void ChatUI::Update(float deltaSeconds)
    {
        m_bubble.Update(static_cast<double>(deltaSeconds));

        m_panel.SetViewportHeight(PanelContentRect().size.y);

        m_panel.Update(static_cast<double>(deltaSeconds));
    }


    // ============================================================
    // 绘制
    // ============================================================

    void ChatUI::Draw(sf::RenderWindow& window)
    {
        // Live2D 用的是原生 OpenGL，画 SFML 之前把状态存起来，画完恢复。
        window.pushGLStates();

        // ---- 气泡 ----
        if (m_fontLoaded && m_bubble.Visible() && !m_lines.empty())
        {
            const float centerX = m_bubbleRect.position.x + m_bubbleRect.size.x / 2.0f;
            const float bottom = m_bubbleRect.position.y + m_bubbleRect.size.y;

            // 小尾巴（指向角色）。
            sf::ConvexShape tail(3);

            tail.setPoint(0, { centerX - kTailHalfWidth, bottom - 1.0f });
            tail.setPoint(1, { centerX + kTailHalfWidth, bottom - 1.0f });
            tail.setPoint(2, { centerX, bottom + kTailHeight });
            tail.setFillColor(kBubbleFill);
            tail.setOutlineColor(kBubbleLine);
            tail.setOutlineThickness(2.0f);

            if (!m_config.bubbleTopRight)
            {
                window.draw(tail);
            }

            window.draw(MakeRoundedRect(
                m_bubbleRect, 14.0f, kBubbleFill, kBubbleLine, 2.0f));

            // 盖住尾巴和气泡接缝处的描边。
            sf::RectangleShape seam(
                { kTailHalfWidth * 2.0f - 2.0f, 4.0f });

            seam.setPosition({ centerX - kTailHalfWidth + 1.0f, bottom - 3.0f });
            seam.setFillColor(kBubbleFill);

            if (!m_config.bubbleTopRight)
            {
                window.draw(seam);
            }

            // 已经显现的文字。
            std::size_t remaining = m_bubble.RevealedCount();

            float y = m_bubbleRect.position.y + kBubblePadding;

            for (const std::u32string& line : m_lines)
            {
                const std::size_t take = std::min(line.size(), remaining);

                remaining -= take;

                if (take > 0)
                {
                    sf::Text text(
                        m_font,
                        sf::String(line.substr(0, take)),
                        m_config.textSize);

                    text.setFillColor(kTextColor);
                    text.setPosition({ m_bubbleRect.position.x + kBubblePadding, y });

                    window.draw(text);
                }

                y += m_lineHeight;
            }

            // 跳过按钮。
            if (m_bubble.CanSkip())
            {
                window.draw(MakeRoundedRect(
                    m_skipRect, 8.0f,
                    m_hoverSkip ? kSkipHover : kSkipFill, kSkipLine, 1.0f));

                const std::string skipCaption = "跳过";

                sf::Text label(m_font, sf::String::fromUtf8(
                    skipCaption.begin(), skipCaption.end()), 14);

                label.setFillColor(kSkipText);

                const sf::FloatRect bounds = label.getLocalBounds();

                label.setPosition({
                    m_skipRect.position.x + (m_skipRect.size.x - bounds.size.x) / 2.0f
                        - bounds.position.x,
                    m_skipRect.position.y + (m_skipRect.size.y - bounds.size.y) / 2.0f
                        - bounds.position.y });

                window.draw(label);
            }
        }

        // ---- 小提示 ----
        if (m_fontLoaded && !m_status.empty())
        {
            window.draw(MakeRoundedRect(
                m_statusRect, 8.0f,
                m_statusIsError ? kErrorFill : kStatusFill,
                m_statusIsError ? kErrorText : kBarLine,
                1.0f));

            sf::Text text(
                m_font,
                sf::String::fromUtf8(m_status.begin(), m_status.end()),
                m_config.statusTextSize);

            text.setFillColor(m_statusIsError ? kErrorText : kStatusText);

            const sf::FloatRect bounds = text.getLocalBounds();

            text.setPosition({
                m_statusRect.position.x + 12.0f - bounds.position.x,
                m_statusRect.position.y + (m_statusRect.size.y - bounds.size.y) / 2.0f
                    - bounds.position.y });

            window.draw(text);
        }

        // ---- 输入框底板 ----
        window.draw(MakeRoundedRect(m_barRect, 12.0f, kBarFill, kBarLine, 2.0f));

        // ---- 发送按钮 ----
        {
            const sf::Color fill = !m_inputEnabled
                ? kButtonOff
                : (m_hoverSend ? kButtonHover : kButtonFill);

            window.draw(MakeRoundedRect(m_sendRect, 12.0f, fill, fill, 0.0f));

            if (m_fontLoaded)
            {
                const std::string caption = "发送";

                sf::Text label(
                    m_font,
                    sf::String::fromUtf8(caption.begin(), caption.end()),
                    m_config.textSize - 2);

                label.setFillColor(kButtonText);

                const sf::FloatRect bounds = label.getLocalBounds();

                label.setPosition({
                    m_sendRect.position.x + (m_sendRect.size.x - bounds.size.x) / 2.0f
                        - bounds.position.x,
                    m_sendRect.position.y + (m_sendRect.size.y - bounds.size.y) / 2.0f
                        - bounds.position.y });

                window.draw(label);
            }
        }

        DrawHistoryPanel(window);

        window.popGLStates();
    }

    // ============================================================
    // 右侧面板
    // ============================================================

    void ChatUI::DrawHistoryPanel(sf::RenderWindow& window)
    {
        const sf::FloatRect panel = PanelRect();

        const bool visible = m_panel.Progress() > 0.0f;

        // ---- 小标签（面板收起时也在）----
        {
            const sf::FloatRect tab = TabRect();

            const sf::Color fill = m_hoverTab ? kButtonHover : kButtonFill;

            window.draw(MakeRoundedRect(tab, 8.0f, fill, fill, 0.0f));

            if (m_fontLoaded)
            {
                // 竖排的“对话”两个字。
                const char32_t chars[2] = { U'对', U'话' };

                for (int i = 0; i < 2; ++i)
                {
                    sf::Text glyph(m_font, sf::String(std::u32string(1, chars[i])), 16);

                    glyph.setFillColor(kButtonText);

                    const sf::FloatRect b = glyph.getLocalBounds();

                    glyph.setPosition({
                        tab.position.x + (tab.size.x - b.size.x) / 2.0f - b.position.x,
                        tab.position.y + 8.0f + 22.0f * static_cast<float>(i) - b.position.y });

                    window.draw(glyph);
                }

                // 排队数量的小红点。
                if (m_panel.QueuedCount() > 0)
                {
                    const sf::FloatRect badge(
                        { tab.position.x + 4.0f, tab.position.y + tab.size.y - 26.0f },
                        { tab.size.x - 8.0f, 20.0f });

                    window.draw(MakeRoundedRect(badge, 10.0f, kBadgeFill, kBadgeFill, 0.0f));

                    sf::Text number(
                        m_font,
                        sf::String(std::to_string(m_panel.QueuedCount())),
                        13);

                    number.setFillColor(kButtonText);

                    const sf::FloatRect b = number.getLocalBounds();

                    number.setPosition({
                        badge.position.x + (badge.size.x - b.size.x) / 2.0f - b.position.x,
                        badge.position.y + (badge.size.y - b.size.y) / 2.0f - b.position.y });

                    window.draw(number);
                }
            }
        }

        if (!visible)
        {
            return;
        }

        // ---- 底板 + 标题 ----
        window.draw(MakeRoundedRect(panel, 10.0f, kPanelFill, kPanelLine, 2.0f));

        if (!m_fontLoaded)
        {
            return;
        }

        {
            sf::Text title(m_font, sf::String(std::u32string(U"对话")), 18);

            title.setFillColor(kPanelTitle);

            const sf::FloatRect b = title.getLocalBounds();

            title.setPosition({
                panel.position.x + 14.0f - b.position.x,
                panel.position.y + (kPanelTitleHeight - b.size.y) / 2.0f - b.position.y });

            window.draw(title);
        }

        // ---- 卡片：用一个只覆盖内容区的视图裁剪，坐标变成“内容区内部坐标” ----
        const sf::FloatRect content = PanelContentRect();

        const sf::View previousView = window.getView();

        const sf::Vector2f windowSize(
            static_cast<float>(m_windowSize.x),
            static_cast<float>(m_windowSize.y));

        sf::View view(sf::FloatRect({ 0.0f, 0.0f }, content.size));

        view.setViewport(sf::FloatRect(
            { content.position.x / windowSize.x, content.position.y / windowSize.y },
            { content.size.x / windowSize.x, content.size.y / windowSize.y }));

        window.setView(view);

        const PanelMetrics& metrics = m_panel.Metrics();

        const float scroll = m_panel.ScrollY();

        for (const CardLayout& card : m_panel.Cards())
        {
            const float top = card.top - scroll;

            if (top + card.height < 0.0f || top > content.size.y)
            {
                continue;
            }

            const bool active = card.status == ItemStatus::Speaking;

            const sf::FloatRect cardRect(
                { metrics.padding, top },
                { metrics.width - 2.0f * metrics.padding, card.height });

            window.draw(MakeRoundedRect(
                cardRect, 8.0f, kCardFill, active ? kCardActiveLine : kCardLine, 2.0f));

            float y = top + metrics.cardPadding;

            const float textX = cardRect.position.x + metrics.cardPadding;

            const bool dim = card.status == ItemStatus::Cancelled
                || card.status == ItemStatus::Failed;

            auto drawLine = [&](const std::u32string& line, float lineY, const sf::Color& color)
            {
                sf::Text t(m_font, sf::String(line), kPanelTextSize);

                t.setFillColor(color);

                t.setPosition({ textX, lineY + 1.0f });

                window.draw(t);
            };

            if (card.hasHeader)
            {
                const sf::Color chipColor =
                    card.status == ItemStatus::Failed ? kChipBad : kChipText;

                sf::Text chip(m_font, sf::String(card.chip), 14);

                chip.setFillColor(chipColor);

                const sf::FloatRect b = chip.getLocalBounds();

                chip.setPosition({
                    textX - b.position.x,
                    y + (metrics.headerHeight - b.size.y) / 2.0f - b.position.y });

                window.draw(chip);

                if (card.button != CardButton::None)
                {
                    const sf::FloatRect button(
                        { card.buttonRect.x, card.buttonRect.y - scroll },
                        { card.buttonRect.w, card.buttonRect.h });

                    const bool hover = m_hoverButtonId == card.id;

                    const bool skip = card.button == CardButton::Skip;

                    const sf::Color fill = skip
                        ? (hover ? kSkipHover : kSkipFill)
                        : (hover ? kSkipHover : kSkipFill);

                    window.draw(MakeRoundedRect(button, 6.0f, fill, kSkipLine, 1.0f));

                    sf::Text label(
                        m_font,
                        sf::String(std::u32string(skip ? U"跳过" : U"取消")),
                        14);

                    label.setFillColor(kSkipText);

                    const sf::FloatRect lb = label.getLocalBounds();

                    label.setPosition({
                        button.position.x + (button.size.x - lb.size.x) / 2.0f - lb.position.x,
                        button.position.y + (button.size.y - lb.size.y) / 2.0f - lb.position.y });

                    window.draw(label);
                }

                y += metrics.headerHeight;
            }

            for (const std::u32string& line : card.userLines)
            {
                drawLine(line, y, dim ? kCardDimText : kTextColor);

                y += metrics.lineHeight;
            }

            for (const std::u32string& line : card.replyLines)
            {
                drawLine(line, y, dim ? kCardDimText : kPanelTitle);

                y += metrics.lineHeight;
            }
        }

        // ---- 没有内容时的提示 ----
        if (m_panel.Cards().empty())
        {
            sf::Text hint(m_font, sf::String(std::u32string(U"还没有对话")), 15);

            hint.setFillColor(kCardDimText);

            hint.setPosition({ 16.0f, 12.0f });

            window.draw(hint);
        }

        window.setView(previousView);
    }
}
