#pragma once

#include <string>
#include <vector>
#include <filesystem>

const char* ApplicationFilePath();
std::string ReadFileAsString(const std::string& path);
bool ReadBinaryFile(const std::string& path, std::vector<char>& bin);