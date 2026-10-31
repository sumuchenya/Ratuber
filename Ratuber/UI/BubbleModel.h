#pragma once

#include <functional>
#include <string>
#include <vector>

// ============================================================
// BubbleModel.h
//
// 职责：气泡里的文字怎么“显现”。只有逻辑，不依赖 SFML 和 Windows，
//       所以可以单独测试。
//
// 规则：
//   - 轮到播放时（Start），气泡出现，文字是空的。
//   - 每个小句开始播放（AddSegment），这个小句的字才允许显现，
//     以正常速度逐字出现，和声音大致同步。
//   - 播放结束或被跳过（Finish），没显现完的字用超高速逐字补完。
//   - 补完后再停留一会儿，然后气泡消失。
//
// 显示的文字永远是“AI 完整回复的前 N 个字”，气泡的大小从一开始
// 就按完整回复排好，不会一边显现一边变大。
// ============================================================

namespace Ratuber::UI
{
    // UTF-8 <-> UTF-32（不依赖 SFML）。非法的字节会被换成 U+FFFD。
    std::u32string Utf8ToUtf32(const std::string& text);

    std::string Utf32ToUtf8(const std::u32string& text);

    // 按像素宽度折行。
    //   measure：返回一个字符的像素宽度。
    //   遇到 '\n' 强制换行；
    //   “，。！？”这类标点不会排在行首（宁可这一行多溢出一个字）；
    //   一行至少放一个字，所以再窄的宽度也不会死循环。
    std::vector<std::u32string> WrapText(
        const std::u32string& text,
        float maxWidth,
        const std::function<float(char32_t)>& measure
    );

    class BubbleModel
    {
    public:
        enum class Phase
        {
            Hidden,

            // 在播放，文字随声音显现。
            Speaking,

            // 播放结束，正在把剩下的字补完。
            Finishing,

            // 字已经全部显现，停留一会儿。
            Lingering,
        };

        // 速度，单位：字 / 秒。
        static constexpr double kSpeakingSpeed = 9.0;    // 声音里每个小句的显现速度
        static constexpr double kFinishSpeed = 150.0;    // 播放结束 / 被跳过后补字
        static constexpr double kTextOnlySpeed = 45.0;   // 没有声音时（只有文字）

        // 全部显现后停留的时间（秒）：每个字加一点，有上下限。
        static constexpr double kLingerMin = 4.0;
        static constexpr double kLingerMax = 12.0;
        static constexpr double kLingerPerChar = 0.12;

        // 轮到播放。fullText = AI 的完整回复。
        void Start(const std::u32string& fullText);

        // 一个小句开始播放。segmentText 是这个小句的文字。
        void AddSegment(const std::u32string& segmentText);

        // 播放结束。skipped 为 true 表示被用户跳过。
        // fullText 是完整回复（没有收到 Start 时也能直接用）。
        void Finish(const std::u32string& fullText, bool skipped);

        void Hide();

        void Update(double deltaSeconds);

        Phase GetPhase() const { return m_phase; }

        bool Visible() const { return m_phase != Phase::Hidden; }

        // 正在播放，可以点“跳过”。
        bool CanSkip() const { return m_phase == Phase::Speaking; }

        // 气泡排版用的完整文字。
        const std::u32string& FullText() const { return m_full; }

        // 已经显现的字数。
        size_t RevealedCount() const;

        // 已经显现的文字（完整回复的前 N 个字）。
        std::u32string VisibleText() const;

    private:
        Phase m_phase = Phase::Hidden;

        std::u32string m_full;

        // 现在允许显现到第几个字。
        size_t m_allowed = 0;

        double m_revealed = 0.0;

        double m_speed = kSpeakingSpeed;

        bool m_sawSegment = false;

        double m_lingerLeft = 0.0;
    };
}
