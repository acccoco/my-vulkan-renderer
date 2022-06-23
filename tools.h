#pragma once

#include <vector>
#include <fstream>

inline std::vector<char> read_file(const std::string &filename)
{
    // ate：从文件末尾开始读取
    // binary：视为二进制文件
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("failed to open file!");

    // 当前指针位于文件末尾，可以方便地获取文件大小
    size_t            file_size = (size_t) file.tellg();
    std::vector<char> buffer(file_size);

    // 从头开始读取
    file.seekg(0);
    file.read(buffer.data(), file_size);

    file.close();
    return buffer;
}