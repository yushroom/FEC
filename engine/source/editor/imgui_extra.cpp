#include "imgui_extra.hpp"


#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

namespace ImGui {
    // Render a rectangle shaped with optional rounding and borders
    void RenderFrame2(ImVec2 p_min, ImVec2 p_max, ImU32 fill_col, bool border,
                      float rounding, ImDrawCornerFlags corner_flags) {
        ImGuiContext &g = *GImGui;
        ImGuiWindow *window = g.CurrentWindow;
        window->DrawList->AddRectFilled(p_min, p_max, fill_col, rounding,
                                        corner_flags);
        //		const float border_size = g.Style.FrameBorderSize;
        const float border_size = 1.0f;
        if (border && border_size > 0.0f) {
            window->DrawList->AddRect(p_min + ImVec2(1, 1), p_max + ImVec2(1, 1),
                                      GetColorU32(ImGuiCol_BorderShadow), rounding,
                                      corner_flags, border_size);
            window->DrawList->AddRect(p_min, p_max, GetColorU32(ImGuiCol_Border),
                                      rounding, corner_flags, border_size);
        }
    }

    bool ButtonEx2(const char *label, const ImVec2 &size_arg,
                   ImGuiButtonFlags flags, ImDrawCornerFlags corner_flags,
                   bool active) {
        ImGuiWindow *window = GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext &g = *GImGui;
        const ImGuiStyle &style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = CalcTextSize(label, NULL, true);

        ImVec2 pos = window->DC.CursorPos;
        if ((flags & ImGuiButtonFlags_AlignTextBaseLine) &&
            style.FramePadding.y <
                window->DC.CurrLineTextBaseOffset)  // Try to vertically align
                                                    // buttons that are smaller/have
                                                    // no padding so that text
                                                    // baseline matches (bit hacky,
                                                    // since it shouldn't be a flag)
            pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
        ImVec2 size =
            CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f,
                         label_size.y + style.FramePadding.y * 2.0f);

        const ImRect bb(pos, pos + size);
        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, id)) return false;

        if (window->DC.ItemFlags & ImGuiItemFlags_ButtonRepeat)
            flags |= ImGuiButtonFlags_Repeat;
        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

        // Render
        const ImU32 col =
            GetColorU32((active || (held && hovered))
                            ? ImGuiCol_ButtonActive
                            : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
        RenderNavHighlight(bb, id);
        RenderFrame2(bb.Min, bb.Max, col, true, 4.0f, corner_flags);
        RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding,
                          label, NULL, &label_size, style.ButtonTextAlign, &bb);

        // Automatically close popups
        // if (pressed && !(flags & ImGuiButtonFlags_DontClosePopups) &&
        // (window->Flags & ImGuiWindowFlags_Popup))
        //    CloseCurrentPopup();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.LastItemStatusFlags);
        return pressed;
    }


    bool Button2(const char *label, const ImVec2 &size_arg,
                 ImDrawCornerFlags corner_flags, bool active = false) {
        return ButtonEx2(label, size_arg, 0, corner_flags, active);
    }

    static bool g_begined = false;
    static int g_count = 0;
    static int g_index = 0;

    void BeginSegmentedButtons(int count) {
        assert(!g_begined);
        g_begined = true;
        g_count = count;
        g_index = 0;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(-1, 2));
    }
    bool SegmentedButton(const char *label, ImVec2 size, bool active/* = false*/) {
        ImDrawCornerFlags flag =
            g_index == 0 ? ImDrawCornerFlags_Left
                         : (g_index == g_count - 1 ? ImDrawCornerFlags_Right
                                                   : ImDrawCornerFlags_None);
        g_index++;
        bool pressed = Button2(label, size, flag);
        ImGui::SameLine(0);
        return pressed;
    }
    void EndSegmentedButtons() {
        assert(g_begined);
        assert(g_index == g_count);
        g_begined = false;
        ImGui::PopStyleVar();
    }

}  // namespace ImGui