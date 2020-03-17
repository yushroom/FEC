#ifndef EDITOR_APP_HPP
#define EDITOR_APP_HPP

#include "glfw_app.hpp"

class EditorApp : public GLFWApp {
public:
	void OnEditorUI() override;
};

#endif
