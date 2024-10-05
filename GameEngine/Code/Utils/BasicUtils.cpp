#include "BasicUtils.h"

#include <Windows.h>
#include <fstream>

namespace Engine::Utils
{

    std::wstring stringToWString(const std::string& str) 
    {
        if (str.empty()) return std::wstring();

        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), nullptr, 0);
        std::wstring wstr(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size_needed);

        return wstr;
    }

    std::vector<char> loadBytesFromFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) 
        {
            throw std::runtime_error("Failed to open file: " + filename);
        }

        // Read the file content
        return std::vector<char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    std::vector<std::string> splitString(const std::string& str, char delimiter)
    {
        std::vector<std::string> result;
        std::string::size_type start = 0;
        std::string::size_type end = 0;

        // Loop through the string to find each occurrence of the delimiter
        while ((end = str.find(delimiter, start)) != std::string::npos) {
            // Extract the substring and add it to the vector
            result.push_back(str.substr(start, end - start));
            start = end + 1;  // Move past the delimiter
        }

        // Add the last part of the string after the last delimiter
        result.push_back(str.substr(start));

        return result;
    }

}