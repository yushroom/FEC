#pragma once

#include <vector>
#include <string>

class ProjectSelector {
public:
	ProjectSelector();
	bool OnEditorGUI();

	std::string GetSelectedProjectPath() const {
		return m_SelectedProjectPath;
	}

private:
	int m_SelectedIndex = -1;
	std::string m_SelectedProjectPath;
	std::vector<std::string> m_ProjectPaths;
};