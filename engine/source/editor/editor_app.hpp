#ifndef EDITOR_APP_HPP
#define EDITOR_APP_HPP

#include "glfw_app.hpp"
#include "project_selector.hpp"

class EditorApp : public GLFWApp {
public:
	void OnEditorUI() override;
	
private:
	ProjectSelector m_ProjectSelector;
	bool m_ShowProjectSelector = true;
};

#endif
