#ifndef IMGUI_EXTRA_HPP
#define IMGUI_EXTRA_HPP

#include <imgui.h>

namespace ImGui {

	void BeginSegmentedButtons(int count);
	bool SegmentedButton(const char *label, ImVec2 size, bool active = false);
	void EndSegmentedButtons();
}

#endif /* IMGUI_EXTRA_HPP */