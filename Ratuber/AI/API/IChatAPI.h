#pragma once

#include <iterator>
#include <string>

// ============================================================
// IChatAPI.h
//
// 职责：定义 Ratuber 的 AI 对话接口，以及 AI 回复的格式约定。
//
// 用途：
// 1. 接收用户说的话（语音识别并纠错之后的文本）。
// 2. 请求 AI 生成回复。
// 3. 返回 AI 的回复。回复必须是下面约定格式的 JSON 文本。
//
// AI 回复格式：
//
//   {
//     "text": "太好了！",
//     "emotion": "happy",
//     "max_level": "compose",
//     "client_state": {
//       "busy": false
//     }
//   }
//
//   text          回复内容，50 字以内。只有这一项会送去语音合成。
//   emotion       只能是 kEmotions 里的 10 种情绪之一。
//   max_level     固定 "compose"。
//   busy          固定 false。真正的 busy 由 AIManager 根据周期状态改写，
//                 AI 填什么都会被覆盖。
//
// 注意：
// 本文件只声明接口，不包含具体的 API 请求实现。
// 后续可以由 DeepSeek 或其他语言模型实现此接口。
// 实现者必须把 kChatOutputRules 放进系统提示词，
// 角色设定（人设）写在它前面即可。
// ============================================================

namespace Ratuber
{
    namespace AI
    {
        namespace API
        {
            // ------------------------------------------------
            // 允许的 10 种情绪。ai_server.py 只认这些。
            // ------------------------------------------------
            inline constexpr const char* kEmotions[] =
            {
                "greeting",     // 打招呼
                "happy",        // 开心
                "shy",          // 害羞
                "surprised",    // 惊讶
                "sad",          // 悲伤
                "angry",        // 生气
                "comfort",      // 安慰
                "thank_you",    // 感谢
                "celebrate",    // 庆祝
                "funny_fail",   // 搞笑翻车
            };

            inline constexpr int kEmotionCount =
                static_cast<int>(std::size(kEmotions));


            // ------------------------------------------------
            // 解析、校验之后的 AI 回复。
            // ------------------------------------------------
            struct ChatReply
            {
                // 回复文本。
                std::string text;

                // 情绪。为空表示 AI 没给出合法情绪，此时只播语音、不做动作。
                std::string emotion;

                // "compose" / "preset" / "atom"。
                std::string maxLevel = "compose";
            };


            // ------------------------------------------------
            // 约束 AI 输出格式的提示词。
            // 把它拼进系统提示词。
            // ------------------------------------------------
            inline constexpr const char* kChatOutputRules = R"RULES(
你每次回复都必须只输出一个 JSON 对象，不要输出任何其他文字，不要使用 Markdown 代码块。格式如下：

{"text":"太好了！","emotion":"happy","max_level":"compose","client_state":{"busy":false}}

字段规则：
1. text：你要说的话。口语化，像聊天一样自然。必须控制在 50 个字以内，越短越好。可以用逗号、句号、感叹号断句。不要使用表情符号、颜文字、括号里的动作描写，也不要使用 Markdown。
2. emotion：说这句话时的情绪。只能从下面 10 个里选 1 个，原样写出英文单词，不允许输出任何其他值：
   greeting（打招呼）、happy（开心）、shy（害羞）、surprised（惊讶）、sad（悲伤）、angry（生气）、comfort（安慰）、thank_you（感谢）、celebrate（庆祝）、funny_fail（搞笑翻车）
3. max_level：固定写 "compose"。
4. client_state.busy：固定写 false。
)RULES";


            class IChatAPI
            {
            public:
                virtual ~IChatAPI() = default;

                // ------------------------------------------------
                // 发送用户消息并获取 AI 回复。
                //
                // 参数：
                //   userMessage：用户说的话（已经过语音识别纠错）。
                //
                // 返回：
                //   true：请求成功，reply 中保存 AI 的原始回复文本。
                //   false：请求失败。
                //
                // 输出：
                //   reply：AI 一次性返回的整段回复（不是流式），
                //          内容应当是上面约定格式的 JSON 文本。
                //          这里不需要解析、不需要校验，调用方会处理。
                //   errorMessage：失败时的错误信息。
                //
                // 注意：
                //   1. 多轮对话的历史由具体实现自己维护。
                //   2. 具体实现请求模型时要带上 kChatOutputRules，
                //      如果模型支持 JSON 输出模式（例如 DeepSeek 的
                //      response_format = json_object），建议打开。
                //   3. 调用方在回复格式不合法时会重试一次，
                //      所以实现里不要因为“同一句话被问了两次”而报错。
                // ------------------------------------------------
                virtual bool SendMessage(
                    const std::string& userMessage,
                    std::string& reply,
                    std::string& errorMessage
                ) = 0;

                // ------------------------------------------------
                // 让实现把“这句话以及它的回复”从对话历史里撤掉。
                // 用户取消了一个排队中的任务时调用：这一轮其实没有发生过，
                // AI 不应该以为自己说过。
                // 默认什么都不做；维护了历史的实现请重写它。
                // 可能在任意线程被调用。
                // ------------------------------------------------
                virtual void ForgetExchange(const std::string& userMessage)
                {
                    (void)userMessage;
                }
            };
        }
    }
}
