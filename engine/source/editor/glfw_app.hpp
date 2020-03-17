#ifndef GLFW_APP_HPP
#define GLFW_APP_HPP

#include <GLFW/glfw3.h>
#include <ecs.h>

class GLFWApp {
public:
	virtual int Init();
	virtual void Run();
	virtual void OnEditorUI() = 0;

protected:
	GLFWwindow* m_Window = nullptr;
	World* m_EditorWorld = nullptr;
};

#endif