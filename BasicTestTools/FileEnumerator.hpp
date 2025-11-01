#ifndef FILE_ENUMERATOR_HPP
#define FILE_ENUMERATOR_HPP

#include <vector>
#include <string>
#include <set>
#include <algorithm>
#include <stdexcept>
#include <windows.h>


namespace FileEnumerator {

    /**
     * 获取指定文件夹下匹配后缀的文件列表（完整路径）
     * @param folderPath 文件夹路径
     * @param extensions 文件扩展名集合（不包含点号，如 {"txt", "cpp"}），空集合表示获取所有文件
     * @param recursive 是否递归遍历子文件夹
     * @return 匹配的文件完整路径列表
     * @throws std::runtime_error 当路径无效时抛出异常
     */
    inline std::vector<std::string> getFiles(
            const std::string &folderPath,
            const std::set<std::string> &extensions = {},
            bool recursive = false) {
        std::vector<std::string> result;

        if (folderPath.empty()) {
            throw std::runtime_error("Folder path cannot be empty");
        }

        // 标准化文件夹路径，确保以反斜杠结尾
        std::string searchPath = folderPath;
        if (searchPath.back() != '\\') {
            searchPath += '\\';
        }

        // Windows API 查找文件的搜索模式
        std::string searchPattern = searchPath + "*";

        WIN32_FIND_DATAA findFileData;
        HANDLE hFind = FindFirstFileA(searchPattern.c_str(), &findFileData);

        if (hFind == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            if (error == ERROR_PATH_NOT_FOUND || error == ERROR_FILE_NOT_FOUND) {
                return result; // 返回空列表而不是抛出异常
            }
            throw std::runtime_error("Cannot access folder: " + folderPath);
        }

        // 预处理扩展名集合，转换为小写
        std::set<std::string> lowerExtensions;
        for (const auto &ext : extensions) {
            std::string lowerExt = ext;
            std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
            lowerExtensions.insert(lowerExt);
        }

        do {
            const std::string fileName = findFileData.cFileName;

            // 跳过当前目录和父目录
            if (fileName == "." || fileName == "..") {
                continue;
            }

            const std::string fullPath = searchPath + fileName;

            // 判断是否为目录
            if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // 如果需要递归且是目录，则递归调用
                if (recursive) {
                    try {
                        auto subFiles = getFiles(fullPath, extensions, recursive);
                        result.insert(result.end(), subFiles.begin(), subFiles.end());
                    } catch (const std::exception &) {
                        // 忽略子目录访问错误，继续处理其他目录
                        continue;
                    }
                }
            } else {
                // 是文件，检查扩展名
                bool shouldInclude = false;

                if (extensions.empty()) {
                    // 如果没有指定扩展名，包含所有文件
                    shouldInclude = true;
                } else {
                    // 提取文件扩展名
                    size_t dotPos = fileName.find_last_of('.');
                    if (dotPos != std::string::npos && dotPos < fileName.length() - 1) {
                        std::string fileExt = fileName.substr(dotPos + 1);

                        // 转换为小写进行比较
                        std::transform(fileExt.begin(), fileExt.end(), fileExt.begin(), ::tolower);

                        // 检查扩展名是否在指定集合中
                        shouldInclude = lowerExtensions.find(fileExt) != lowerExtensions.end();
                    }
                }

                if (shouldInclude) {
                    result.push_back(fullPath);
                }
            }
        } while (FindNextFileA(hFind, &findFileData) != 0);

        FindClose(hFind);
        return result;
    }

    /**
     * 获取指定文件夹下匹配后缀的文件相对路径列表（相对于指定文件夹）
     * @param folderPath 文件夹路径
     * @param extensions 文件扩展名集合（不包含点号），空集合表示获取所有文件
     * @param recursive 是否递归遍历子文件夹
     * @return 匹配的文件相对路径列表
     */
    inline std::vector<std::string> getRelativePaths(
            const std::string &folderPath,
            const std::set<std::string> &extensions = {},
            bool recursive = false) {
        auto fullPaths = getFiles(folderPath, extensions, recursive);
        std::vector<std::string> relativePaths;
        relativePaths.reserve(fullPaths.size());

        // 标准化基础路径
        std::string basePath = folderPath;
        if (basePath.back() != '\\') {
            basePath += '\\';
        }

        for (const auto &fullPath : fullPaths) {
            if (fullPath.length() > basePath.length()) {
                relativePaths.push_back(fullPath.substr(basePath.length()));
            }
        }

        return relativePaths;
    }

    /**
     * 获取指定文件夹下匹配后缀的文件名列表（仅文件名，不包含路径）
     * @param folderPath 文件夹路径
     * @param extensions 文件扩展名集合（不包含点号），空集合表示获取所有文件
     * @param recursive 是否递归遍历子文件夹
     * @return 匹配的文件名列表
     */
    inline std::vector<std::string> getFileNames(
            const std::string &folderPath,
            const std::set<std::string> &extensions = {},
            bool recursive = false) {
        auto fullPaths = getFiles(folderPath, extensions, recursive);
        std::vector<std::string> fileNames;
        fileNames.reserve(fullPaths.size());

        for (const auto &fullPath : fullPaths) {
            size_t slashPos = fullPath.find_last_of('\\');
            if (slashPos != std::string::npos) {
                fileNames.push_back(fullPath.substr(slashPos + 1));
            } else {
                fileNames.push_back(fullPath);
            }
        }

        return fileNames;
    }

    /**
     * 检查指定路径是否为目录
     * @param path 路径
     * @return true - 是目录，false - 不是目录
     */
    inline bool isDirectory(const std::string &path) {
        DWORD attributes = GetFileAttributesA(path.c_str());
        return (attributes != INVALID_FILE_ATTRIBUTES) &&
               (attributes & FILE_ATTRIBUTE_DIRECTORY);
    }

    /**
     * 检查指定路径是否存在
     * @param path 路径
     * @return true - 路径存在，false - 路径不存在
     */
    inline bool pathExists(const std::string &path) {
        DWORD attributes = GetFileAttributesA(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES;
    }

    /**
     * 统计匹配文件的数量（不实际加载文件列表，节省内存）
     * @param folderPath 文件夹路径
     * @param extensions 文件扩展名集合
     * @param recursive 是否递归遍历子文件夹
     * @return 匹配文件的数量
     */
    inline size_t countFiles(
            const std::string &folderPath,
            const std::set<std::string> &extensions = {},
            bool recursive = false) {
        size_t count = 0;

        if (folderPath.empty()) {
            return 0;
        }

        std::string searchPath = folderPath;
        if (searchPath.back() != '\\') {
            searchPath += '\\';
        }

        std::string searchPattern = searchPath + "*";

        WIN32_FIND_DATAA findFileData;
        HANDLE hFind = FindFirstFileA(searchPattern.c_str(), &findFileData);

        if (hFind == INVALID_HANDLE_VALUE) {
            return 0;
        }

        // 预处理扩展名集合
        std::set<std::string> lowerExtensions;
        for (const auto &ext : extensions) {
            std::string lowerExt = ext;
            std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
            lowerExtensions.insert(lowerExt);
        }

        do {
            const std::string fileName = findFileData.cFileName;

            if (fileName == "." || fileName == "..") {
                continue;
            }

            const std::string fullPath = searchPath + fileName;

            if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (recursive) {
                    count += countFiles(fullPath, extensions, recursive);
                }
            } else {
                bool shouldInclude = false;

                if (extensions.empty()) {
                    shouldInclude = true;
                } else {
                    size_t dotPos = fileName.find_last_of('.');
                    if (dotPos != std::string::npos && dotPos < fileName.length() - 1) {
                        std::string fileExt = fileName.substr(dotPos + 1);
                        std::transform(fileExt.begin(), fileExt.end(), fileExt.begin(), ::tolower);
                        shouldInclude = lowerExtensions.find(fileExt) != lowerExtensions.end();
                    }
                }

                if (shouldInclude) {
                    ++count;
                }
            }
        } while (FindNextFileA(hFind, &findFileData) != 0);

        FindClose(hFind);
        return count;
    }

} // namespace FileEnumerator


namespace FileSuffixProcess {

    /**
     * 更换单个文件路径的后缀名
     * @param filePath 文件路径（完整路径或文件名）
     * @param newExtension 新的后缀名（不包含点号）
     * @return 更换后缀后的文件路径
     * @throws std::invalid_argument 当文件路径为空或无效时抛出异常
     */
    inline std::string changeFileExtension(const std::string &filePath, const std::string &newExtension) {
        if (filePath.empty()) {
            throw std::invalid_argument("File path cannot be empty");
        }

        if (newExtension.empty()) {
            throw std::invalid_argument("New extension cannot be empty");
        }

        size_t dotPos = filePath.find_last_of('.');
        size_t slashPos = filePath.find_last_of("\\/");

        // 确保点在文件名中（而不是在路径中）
        if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos)) {
            // 有扩展名，替换它
            return filePath.substr(0, dotPos + 1) + newExtension;
        } else {
            // 没有扩展名，添加新扩展名
            return filePath + "." + newExtension;
        }
    }

    /**
     * 批量更换文件路径列表的后缀名
     * @param filePaths 文件路径列表（完整路径或文件名）
     * @param newExtension 新的后缀名（不包含点号）
     * @return 更换后缀后的文件路径列表
     * @throws std::invalid_argument 当新后缀名为空时抛出异常
     */
    inline std::vector<std::string> changeFilesExtension(
            const std::vector<std::string> &filePaths,
            const std::string &newExtension) {
        if (newExtension.empty()) {
            throw std::invalid_argument("New extension cannot be empty");
        }

        std::vector<std::string> result;
        result.reserve(filePaths.size());

        for (const auto &filePath : filePaths) {
            if (!filePath.empty()) {
                result.push_back(changeFileExtension(filePath, newExtension));
            }
        }

        return result;
    }

    /**
     * 获取文件的扩展名（不包含点号）
     * @param filePath 文件路径
     * @return 文件的扩展名，如果没有扩展名则返回空字符串
     */
    inline std::string getFileExtension(const std::string &filePath) {
        if (filePath.empty()) {
            return "";
        }

        size_t dotPos = filePath.find_last_of('.');
        size_t slashPos = filePath.find_last_of("\\/");

        // 确保点在文件名中（而不是在路径中）
        if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos) &&
            dotPos < filePath.length() - 1) {
            return filePath.substr(dotPos + 1);
        }

        return "";
    }

    /**
     * 获取不包含扩展名的文件路径
     * @param filePath 文件路径
     * @return 不包含扩展名的文件路径
     */
    inline std::string getFilePathWithoutExtension(const std::string &filePath) {
        if (filePath.empty()) {
            return "";
        }

        size_t dotPos = filePath.find_last_of('.');
        size_t slashPos = filePath.find_last_of("\\/");

        // 确保点在文件名中（而不是在路径中）
        if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos)) {
            return filePath.substr(0, dotPos);
        }

        return filePath;
    }

    /**
     * 获取包含或不包含扩展名的文件名（仅文件名部分，不包含路径）
     * @param filePath 文件路径
     * @param extension 是否包含扩展名
     * @return 包含或不包含扩展名的文件名
     */
    inline std::string getFileBaseName(const std::string &filePath, bool extension = true) {
        if (filePath.empty()) {
            return "";
        }

        size_t slashPos = filePath.find_last_of("\\/");
        std::string fileName = (slashPos != std::string::npos) ?
                               filePath.substr(slashPos + 1) : filePath;

        if (!extension) {
            size_t dotPos = fileName.find_last_of('.');
            if (dotPos != std::string::npos && dotPos > 0) {
                return fileName.substr(0, dotPos);
            }
        }

        return fileName;
    }

} // namespace FileSuffixProcess


#endif // FILE_ENUMERATOR_HPP
