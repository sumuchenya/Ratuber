#include "BubbleModel.h"

#include <algorithm>
#include <cmath>

namespace Ratuber::UI
{
    // ============================================================
    // UTF-8 <-> UTF-32
    // ============================================================

    std::u32string Utf8ToUtf32(const std::string& text)
    {
        std::u32string out;

        out.reserve(text.size());

        size_t i = 0;

        while (i < text.size())
        {
            const unsigned char c = static_cast<unsigned char>(text[i]);

            char32_t cp = 0xFFFD;

            size_t length = 1;

            if (c < 0x80)
            {
                cp = c;
            }
            else if ((c & 0xE0) == 0xC0)
            {
                length = 2;
                cp = c & 0x1F;
            }
            else if ((c & 0xF0) == 0xE0)
            {
                length = 3;
                cp = c & 0x0F;
            }
            else if ((c & 0xF8) == 0xF0)
            {
                length = 4;
                cp = c & 0x07;
            }
            else
            {
                out.push_back(0xFFFD);
                ++i;

                continue;
            }

            if (i + length > text.size())
            {
                out.push_back(0xFFFD);

                break;
            }

            bool valid = true;

            for (size_t k = 1; k < length; ++k)
            {
                const unsigned char cc = static_cast<unsigned char>(text[i + k]);

                if ((cc & 0xC0) != 0x80)
                {
                    valid = false;

                    break;
                }

                cp = (cp << 6) | (cc & 0x3F);
            }

            if (!valid)
            {
                out.push_back(0xFFFD);
                ++i;

                continue;
            }

            out.push_back(cp);

            i += length;
        }

        return out;
    }

    std::string Utf32ToUtf8(const std::u32string& text)
    {
        std::string out;

        out.reserve(text.size() * 3);

        for (const char32_t cp : text)
        {
            if (cp < 0x80)
            {
                out += static_cast<char>(cp);
            }
            else if (cp < 0x800)
            {
                out += static_cast<char>(0xC0 | (cp >> 6));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
            else if (cp < 0x10000)
            {
                out += static_cast<char>(0xE0 | (cp >> 12));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
            else
            {
                out += static_cast<char>(0xF0 | (cp >> 18));
                out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
        }

        return out;
    }


    // ============================================================
    // 折行
    // ============================================================

    namespace
    {
        // 不能排在行首的标点。
        bool CannotStartLine(char32_t c)
        {
            switch (c)
            {
            case U'，': case U'。': case U'！': case U'？':
            case U'、': case U'；': case U'：': case U'）':
            case U'”': case U'’': case U'…': case U'》':
            case U',': case U'.': case U'!': case U'?':
            case U';': case U':': case U')':
                return true;

            default:
                return false;
            }
        }
    }

    std::vector<std::u32string> WrapText(
        const std::u32string& text,
        float maxWidth,
        const std::function<float(char32_t)>& measure
    )
    {
        std::vector<std::u32string> lines;

        std::u32string current;

        float width = 0.0f;

        for (const char32_t c : text)
        {
            if (c == U'\r')
            {
                continue;
            }

            if (c == U'\n')
            {
                lines.push_back(current);

                current.clear();
                width = 0.0f;

                continue;
            }

            const float w = measure(c);

            const bool full = !current.empty() && width + w > maxWidth;

            // 标点宁可溢出一点，也不排到行首。
            if (full && !CannotStartLine(c))
            {
                lines.push_back(current);

                current.clear();
                width = 0.0f;
            }

            current.push_back(c);

            width += w;
        }

        if (!current.empty() || lines.empty())
        {
            lines.push_back(current);
        }

        return lines;
    }


    // ============================================================
    // BubbleModel
    // ============================================================

    void BubbleModel::Start(const std::u32string& fullText)
    {
        m_full = fullText;

        m_allowed = 0;
        m_revealed = 0.0;
        m_speed = kSpeakingSpeed;
        m_sawSegment = false;
        m_lingerLeft = 0.0;

        m_phase = Phase::Speaking;
    }

    void BubbleModel::AddSegment(const std::u32string& segmentText)
    {
        if (m_phase != Phase::Speaking)
        {
            return;
        }

        m_sawSegment = true;

        m_allowed = std::min(m_full.size(), m_allowed + segmentText.size());
    }

    void BubbleModel::Finish(const std::u32string& fullText, bool skipped)
    {
        // 没收到 Start（比如事件丢了）：当作刚开始。
        if (m_phase == Phase::Hidden || m_phase == Phase::Lingering)
        {
            Start(fullText);
        }

        m_full = fullText;

        m_allowed = m_full.size();

        m_revealed = std::min(m_revealed, static_cast<double>(m_full.size()));

        // 被跳过、或者已经有过声音：超高速补字。
        // 没有声音（TTS 没出来）：只是文字，用比较舒服的速度。
        m_speed = (skipped || m_sawSegment) ? kFinishSpeed : kTextOnlySpeed;

        m_phase = Phase::Finishing;

        // 本来就已经全部显现了（或者是空文本）：直接进入停留。
        if (m_revealed >= static_cast<double>(m_full.size()))
        {
            m_phase = Phase::Lingering;

            m_lingerLeft = std::clamp(
                kLingerPerChar * static_cast<double>(m_full.size()),
                kLingerMin,
                kLingerMax
            );
        }
    }

    void BubbleModel::Hide()
    {
        m_phase = Phase::Hidden;

        m_full.clear();

        m_allowed = 0;
        m_revealed = 0.0;
    }

    void BubbleModel::Update(double deltaSeconds)
    {
        if (deltaSeconds <= 0.0)
        {
            return;
        }

        if (m_phase == Phase::Speaking || m_phase == Phase::Finishing)
        {
            m_revealed = std::min(
                static_cast<double>(m_allowed),
                m_revealed + m_speed * deltaSeconds
            );

            if (m_phase == Phase::Finishing
                && m_revealed >= static_cast<double>(m_full.size()))
            {
                m_phase = Phase::Lingering;

                m_lingerLeft = std::clamp(
                    kLingerPerChar * static_cast<double>(m_full.size()),
                    kLingerMin,
                    kLingerMax
                );
            }
        }
        else if (m_phase == Phase::Lingering)
        {
            m_lingerLeft -= deltaSeconds;

            if (m_lingerLeft <= 0.0)
            {
                Hide();
            }
        }
    }

    size_t BubbleModel::RevealedCount() const
    {
        return std::min(
            m_full.size(),
            static_cast<size_t>(std::floor(m_revealed + 1e-9))
        );
    }

    std::u32string BubbleModel::VisibleText() const
    {
        return m_full.substr(0, RevealedCount());
    }
}
