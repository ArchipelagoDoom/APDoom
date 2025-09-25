#include <imgui/imgui.h>

#include <forward_list>

class OnScreenMessages
{
    struct Message
    {
        ImVec4 bgcolor;
        ImS32 ttl;
        std::string text;
    };
    std::forward_list<Message> messages;

    OnScreenMessages() {}

    void DoRender()
    {
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        ImGuiViewport* vp = ImGui::GetMainViewport();

        float x = 8.0f;
        float y = vp->Pos.y + vp->Size.y;
        for (Message &message : messages)
        {
            float alpha = (--message.ttl / 30.0f);
            ImColor text_color(1.0f, 1.0f, 1.0f, alpha);
            ImVec2 text_bounds = ImGui::CalcTextSize(message.text.c_str());

            y -= (text_bounds.y + 6.0f);
            message.bgcolor.w = alpha;
            fg->AddRectFilled({x - 4.0f, y - 2.0f}, {x + text_bounds.x + 4.0f, y + text_bounds.y + 2.0f}, ImGui::ColorConvertFloat4ToU32(message.bgcolor), 2.0f, 0);
            fg->AddText({x, y}, text_color, &message.text.front(), (&message.text.back() + 1));
        }
        messages.remove_if([](Message& m){ return m.ttl <= 0; });
    }

    void DoAdd(const std::string text, ImVec4 bgcolor)
    {
        Message new_msg;
        new_msg.text = text;
        new_msg.ttl = 300;
        new_msg.bgcolor = bgcolor;
        messages.push_front(new_msg);
    }

    static OnScreenMessages& Get()
    {
        static OnScreenMessages instance;
        return instance;
    }

public:
    static void Render()
    {
        OnScreenMessages::Get().DoRender();
    }

    static void Add(const std::string text, ImVec4 bgcolor = ImVec4(0.14f, 0.14f, 0.14f, 1.0f))
    {
        OnScreenMessages::Get().DoAdd(text, bgcolor);
    }

    static void AddError(const std::string text)
    {
        OnScreenMessages::Get().DoAdd(text, ImColor(0.3f, 0.0f, 0.0f));
    }

    static void AddWarning(const std::string text)
    {
        OnScreenMessages::Get().DoAdd(text, ImColor(0.3f, 0.3f, 0.0f));
    }

    static void AddNotice(const std::string text)
    {
        OnScreenMessages::Get().DoAdd(text, ImColor(0.0f, 0.3f, 0.0f));
    }

    OnScreenMessages(OnScreenMessages const&) = delete;
    void operator=(OnScreenMessages const&) = delete;
};
