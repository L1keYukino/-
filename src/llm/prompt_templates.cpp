#include "src/llm/prompt_templates.hpp"

namespace vim {

PromptCatalog::PromptCatalog() {
    error_correction_ = {
        IntentType::General,
        "Error Correction",
        "Post-process ASR output: fix homophones, punctuation, capitalization",
        "You are a speech recognition post-processor. Your task is to correct "
        "ASR transcription errors (homophones, missing punctuation, capitalization) "
        "while preserving the speaker's exact wording and intent. Do NOT rephrase, "
        "summarize, or add information. Output only the corrected text with no commentary.",
        {}
    };

    templates_ = {
        {
            IntentType::General,
            "General Dictation",
            "Format spoken text with proper punctuation and paragraph breaks",
            "You are a dictation assistant. Format the user's spoken text with "
            "proper punctuation and paragraph breaks. Fix any obvious ASR errors "
            "(wrong characters, missing punctuation). Output only the formatted text "
            "with no additional commentary.",
            {}
        },
        {
            IntentType::Email,
            "Email",
            "Format dictation as a professional email",
            "You are an email formatting assistant. Format the user's dictation as "
            "a professional email.\n\n"
            "Rules:\n"
            "- First line: 'Subject: <brief subject>'\n"
            "- Blank line after subject\n"
            "- Appropriate greeting (e.g. 'Dear ...' or 'Hi ...')\n"
            "- Body with proper paragraphs\n"
            "- Appropriate closing and signature placeholder\n"
            "- Keep tone professional and concise\n"
            "Output only the formatted email, no commentary.",
            {
                {"ask my boss for a sick day tomorrow",
                 "Subject: Sick Leave Request - Tomorrow\n\n"
                 "Dear [Boss's Name],\n\n"
                 "I am writing to request a sick day for tomorrow as I am not "
                 "feeling well. I will ensure any urgent matters are handled "
                 "before then.\n\n"
                 "Best regards,\n"
                 "[Your Name]"},
            }
        },
        {
            IntentType::Chat,
            "Chat Message",
            "Format dictation as a casual chat message",
            "You are a chat message formatter. Format the user's dictation as a "
            "natural, conversational chat message. Keep it brief and casual. "
            "Use appropriate tone for informal messaging. "
            "Output only the formatted message, no commentary.",
            {}
        },
        {
            IntentType::CodeComment,
            "Code Comment",
            "Format dictation as a clear code comment",
            "You are a code comment formatter. Format the user's dictation as a "
            "clear, concise code comment. Wrap text to approximately 80 characters. "
            "Use // style for single-line comments, /* */ for multi-line. "
            "Output only the comment text (including comment markers), no commentary.",
            {}
        },
        {
            IntentType::Documentation,
            "Documentation",
            "Format dictation as technical documentation",
            "You are a technical documentation formatter. Format the user's dictation "
            "as clear technical documentation. Use proper headings, bullet points "
            "(with '-' prefix), and precise technical language. "
            "Output only the formatted documentation, no commentary.",
            {}
        },
        {
            IntentType::Command,
            "CLI Command",
            "Convert spoken description into the correct CLI command",
            "You are a CLI command formatter. Convert the user's spoken description "
            "into the correct command-line command with appropriate flags. "
            "Default to PowerShell syntax on Windows, bash on Linux/Mac unless "
            "context indicates otherwise. Output only the command, no explanation.",
            {
                {"find all text files in the current directory recursively",
                 "Get-ChildItem -Recurse -Filter *.txt"},
                {"list all running processes sorted by memory usage",
                 "Get-Process | Sort-Object -Property WS -Descending"},
            }
        },
    };
}

const PromptTemplate& PromptCatalog::get(IntentType intent) const {
    for (const auto& t : templates_) {
        if (t.intent == intent) return t;
    }
    return templates_[0]; // fallback to General
}

std::vector<LLMMessage> PromptCatalog::build_messages(
    IntentType intent,
    const std::vector<LLMMessage>& history,
    const std::string& raw_asr_text) const
{
    std::vector<LLMMessage> messages;
    const auto& tmpl = get(intent);

    // System prompt
    messages.push_back({"system", tmpl.system_prompt});

    // Few-shot examples
    for (const auto& [user, assistant] : tmpl.few_shot_examples) {
        messages.push_back({"user", user});
        messages.push_back({"assistant", assistant});
    }

    // Conversation history
    for (const auto& h : history) {
        messages.push_back(h);
    }

    // Current user input
    messages.push_back({"user", raw_asr_text});

    return messages;
}

} // namespace vim
