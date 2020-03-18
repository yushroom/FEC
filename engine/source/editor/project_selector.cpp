#include "project_selector.hpp"

#include <fstream>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <nfd.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <fs.hpp>

static std::string ProjectJsonPath() {
    std::filesystem::path path = ApplicationFilePath();
    path = path / "projects.json";
    return path.string();
}

ProjectSelector::ProjectSelector()
{
    std::filesystem::path path = ProjectJsonPath();
    if (std::filesystem::exists(path)) {
        auto contents = ReadFileAsString(path.string());
        rapidjson::Document d;
        d.Parse(contents.c_str());

        if (d.IsArray()) {
            for (auto& p : d.GetArray()) {
                std::filesystem::path project_path = p.GetString();
                if (std::filesystem::is_directory(project_path)) {
                    project_path = project_path.lexically_normal();
                    m_ProjectPaths.push_back(project_path.string());
                }
            }
        }
    }
}

bool ProjectSelector::OnEditorGUI()
{
    bool ret = false;
    bool clicked1 = false;
    auto size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::Begin("Model selector");
    clicked1 = ImGui::Button("Open Other Project");
    ImGui::SameLine();
    ImGui::Button("New Project");

    //ImGui::BeginChild("Projects", ImVec2(0, 200));
    for (int i = 0; i < m_ProjectPaths.size(); ++i) {
        auto& p = m_ProjectPaths[i];
        bool selected = (i == m_SelectedIndex);
        if (ImGui::Selectable(p.c_str(), &selected)) {
            m_SelectedIndex = i;
        }
    }
    //ImGui::EndChild();

    if (m_SelectedIndex >= 0 && m_SelectedIndex < m_ProjectPaths.size()) {
        ret = ImGui::Button("Open");
    }

    ImGui::End();

    if (clicked1) {
        nfdchar_t* outPath = NULL;
        nfdresult_t result = NFD_PickFolder(NULL, &outPath);

        std::string path;
        if (result == NFD_OKAY) {
            path = outPath;
            free(outPath);
        }
        m_SelectedProjectPath = path;
        m_ProjectPaths.push_back(path);
    }

    if (ret) {
        rapidjson::StringBuffer sb;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
        writer.StartArray();
        for (auto& p : m_ProjectPaths) {
            writer.String(p.c_str());
        }
        writer.EndArray();

        std::ofstream fout(ProjectJsonPath(), std::ostream::out);
        if (fout.is_open()) {
            fout << sb.GetString();
        }
    }

    return ret;
}
