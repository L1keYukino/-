#include "src/llm/prompt_templates.hpp"

namespace vim {

PromptCatalog::PromptCatalog() {
    error_correction_ = {
        IntentType::General,
        "Error Correction",
        "Fix ASR errors only, don't change meaning",
        "你是一个语音纠错器。只修正明显的语音识别错误（错字、漏字、多字），"
        "保留原话的意思和风格。不要改写内容。只输出纠错后的文本。",
        {}
    };

    templates_ = {
        // ─── General: smart formatting ─────────────────
        {
            IntentType::General,
            "智能输出",
            "整理语音文字，必要时改变格式",
            "你是语音转文字工具。输入是语音识别的原始文本。\n\n"
            "默认行为：修正错字、加标点，原样输出。\n\n"
            "仅在以下情况改变输出格式：\n"
            "- 文本明确要求写邮件（如「帮我写邮件」「发给老板」）→ 输出完整邮件\n"
            "- 文本明确要求总结（如「总结一下」「归纳要点」）→ 输出「- 」清单\n"
            "- 文本是长段讨论/会议记录（>50字且含多个要点）→ 输出「- 」清单\n\n"
            "重要：你不是聊天机器人。用户说「你好」「喂」不是说给你听的，照原样输出。\n"
            "不要问候、不要回复、不要问「有什么可以帮你」。\n"
            "直接输出结果。",
            {}
        },
        // ─── Email: format as professional email ────────
        {
            IntentType::Email,
            "写邮件",
            "根据口述内容生成完整邮件",
            "将以下口述内容整理为一封正式邮件：\n"
            "Subject: [简短标题]\n"
            "\n"
            "[合适称呼]\n"
            "\n"
            "[正式正文]\n"
            "\n"
            "[落款]",
            {
                {"发烧了请假一天",
                 "Subject: 请假申请\n\n领导您好，\n\n我因发烧身体不适，今天无法到岗，申请请假一天。请批准。\n\n祝好，\n小王"},
            }
        },
        // ─── Chat ───────────────────────────────────────
        {
            IntentType::Chat,
            "聊天",
            "整理成口语化聊天消息",
            "把用户说的话整理成自然的聊天消息，加上合适标点。保持口语化。只输出消息。",
            {}
        },
        // ─── CodeComment ────────────────────────────────
        {
            IntentType::CodeComment,
            "代码注释",
            "整理成代码注释",
            "把用户对代码的描述整理成简洁的代码注释。用 // 格式。只输出注释。",
            {}
        },
        // ─── Documentation ──────────────────────────────
        {
            IntentType::Documentation,
            "文档",
            "整理成技术文档条目",
            "把用户的描述整理成结构化的文档条目。用 Markdown 格式。只输出文档。",
            {}
        },
        // ─── Command ────────────────────────────────────
        {
            IntentType::Command,
            "命令",
            "转成终端命令",
            "把用户描述的操作转成对应的命令行。默认用 PowerShell。只输出命令。",
            {
                {"查找所有txt文件",
                 "Get-ChildItem -Recurse -Filter *.txt"},
            }
        },
        // ─── Summary ────────────────────────────────────
        {
            IntentType::Summary,
            "总结",
            "提取要点，列成清单",
            "从用户说的话中提取关键信息，用「- 」开头的清单列出。忽略口语冗余。只输出清单。",
            {
                {"下周一上午十点开会讨论新版本，张三准备演示，李四准备测试报告，我来通知大家",
                 "- 下周一上午 10:00 开会\n- 议题：新版本发布\n- 张三：准备演示\n- 李四：准备测试报告\n- 我：通知大家"},
            }
        },
    };
}

const PromptTemplate& PromptCatalog::get(IntentType intent) const {
    for (const auto& t : templates_) {
        if (t.intent == intent) return t;
    }
    return templates_[0];
}

std::vector<LLMMessage> PromptCatalog::build_messages(
    IntentType intent,
    const std::vector<LLMMessage>& history,
    const std::string& raw_asr_text) const
{
    std::vector<LLMMessage> messages;
    const auto& tmpl = get(intent);

    messages.push_back({"system", tmpl.system_prompt});

    for (const auto& [user, assistant] : tmpl.few_shot_examples) {
        messages.push_back({"user", user});
        messages.push_back({"assistant", assistant});
    }

    for (const auto& h : history) {
        messages.push_back(h);
    }

    messages.push_back({"user", raw_asr_text});

    return messages;
}

} // namespace vim
