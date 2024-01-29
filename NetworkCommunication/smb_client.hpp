#ifndef SMB_CLIENT_HPP
#define SMB_CLIENT_HPP

#include <windows.h>
#include <winnetwk.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

#pragma comment(lib, "Mpr.lib")


class SmbClient {
public:
    SmbClient(const std::string &server, const std::string &username, const std::string &password)
            : m_server_w(string2wstring(server)),
              m_username_w(string2wstring(username)),
              m_password_w(string2wstring(password)) {
        if (m_server_w.back() == L'\\') m_server_w = m_server_w.substr(0, m_server_w.size() - 1);
    }

    SmbClient() = default;

    ~SmbClient() = default;

public:
    /**
     * @brief setParam
     *
     * @param server Example: R"(\\ip\path)"
     * @param username
     * @param password
     * @return
     */
    void setParam(const std::string &server, const std::string &username, const std::string &password) {
        m_server_w = string2wstring(server);
        m_username_w = string2wstring(username);
        m_password_w = string2wstring(password);

        if (m_server_w.back() == L'\\') m_server_w = m_server_w.substr(0, m_server_w.size() - 1);
    }

    /**
     * @brief uploadFile
     *
     * @param local_path
     * @param remote_path The relative path of the resource with server as the root directory, example: R"(\path)"
     * @return
     */
    bool uploadFile(const std::string &local_path, const std::string &remote_path) {
        if (connectToServer()) {
            bool status = uploadFileKernel(local_path, remote_path);

            disconnect();
            return status;
        }

        return false;
    }

    /**
     * @brief uploadFiles
     *
     * @param local_paths
     * @param remote_paths The relative path of the resource with server as the root directory, example: R"(\path)"
     * @return
     */
    bool uploadFiles(const std::vector<std::string> &local_paths, const std::vector<std::string> &remote_paths) {
        if (local_paths.size() != remote_paths.size()) return false;

        if (connectToServer()) {
            for (int i = 0; i < local_paths.size(); i++) {
                std::string local_path = local_paths[i];
                std::string remote_path = remote_paths[i];

                bool status = uploadFileKernel(local_path, remote_path);

                if (!status) {
                    disconnect();
                    return false;
                }
            }

            disconnect();
            return true;
        }

        return false;
    }

    /**
     * @brief uploadFiles
     *
     * @param path_pairs { local_path, remote_path }
     * @return
     */
    bool uploadFiles(const std::unordered_map<std::string, std::string> &path_pairs) {
        if (connectToServer()) {
            for (auto &path_pair : path_pairs) {
                std::string local_path = path_pair.first;
                std::string remote_path = path_pair.second;

                bool status = uploadFileKernel(local_path, remote_path);

                if (!status) {
                    disconnect();
                    return false;
                }
            }

            disconnect();
            return true;
        }

        return false;
    }

    /**
     * @brief downloadFile
     *
     * @param remote_path The relative path of the resource with server as the root directory, example: R"(\path)"
     * @param local_path
     * @return
     */
    bool downloadFile(const std::string &remote_path, const std::string &local_path) {
        if (connectToServer()) {
            bool status = downloadFileKernel(remote_path, local_path);

            disconnect();
            return status;
        }

        return false;
    }

    /**
     * @brief downloadFiles
     *
     * @param remote_paths The relative path of the resource with server as the root directory, example: R"(\path)"
     * @param local_paths
     * @return
     */
    bool downloadFiles(const std::vector<std::string> &remote_paths, const std::vector<std::string> &local_paths) {
        if (remote_paths.size() != local_paths.size()) return false;

        if (connectToServer()) {
            for (int i = 0; i < remote_paths.size(); i++) {
                std::string remote_path = remote_paths[i];
                std::string local_path = local_paths[i];

                bool status = downloadFileKernel(remote_path, local_path);

                if (!status) {
                    disconnect();
                    return false;
                }
            }

            disconnect();
            return true;
        }

        return false;
    }

    /**
     * @brief downloadFiles
     *
     * @param path_pairs { remote_path, local_path }
     * @return
     */
    bool downloadFiles(const std::unordered_map<std::string, std::string> &path_pairs) {
        if (connectToServer()) {
            for (auto &path_pair : path_pairs) {
                std::string remote_path = path_pair.first;
                std::string local_path = path_pair.second;

                bool status = downloadFileKernel(remote_path, local_path);

                if (!status) {
                    disconnect();
                    return false;
                }
            }

            disconnect();
            return true;
        }

        return false;
    }

private:
    bool connectToServer() {
        if (m_server_w.empty() || m_username_w.empty() || m_password_w.empty()) return false;

        NETRESOURCEW nr;
        nr.dwType = RESOURCETYPE_DISK;
        nr.lpLocalName = nullptr;
        nr.lpRemoteName = const_cast<wchar_t *>(m_server_w.c_str());
        nr.lpProvider = nullptr;

        DWORD status = WNetAddConnection2W(&nr, m_password_w.c_str(), m_username_w.c_str(), 0);

        if (status == NO_ERROR) {
            std::cout << "SMB connect succeeded!" << std::endl;
        } else {
            std::cout << "SMB connect failed! Code: " << std::to_string(status) << std::endl;
        }

        return (status == NO_ERROR);
    }

    inline void disconnect() {
        WNetCancelConnection2W(m_server_w.c_str(), 0, TRUE);
    }

    static bool checkResourcesFolder(const std::wstring &path) {
        std::wstring folder_path = path.substr(0, path.find_last_of(L'\\'));

        DWORD attributes = GetFileAttributesW(folder_path.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            return true;
        } else {
            if (checkResourcesFolder(folder_path)) {
                return CreateDirectoryW(folder_path.c_str(), nullptr);
            } else {
                return false;
            }
        }
    }

    bool uploadFileKernel(const std::string &local_path, const std::string &remote_path) {
        if (remote_path.empty() || local_path.empty()) return false;

        std::wstring local_path_w = string2wstring(local_path);
        std::wstring remote_path_w = string2wstring(remote_path);

        // refactor remote path
        if (remote_path_w.front() != L'\\') remote_path_w = L"\\" + remote_path_w;
        remote_path_w = m_server_w + remote_path_w;

        // Determine whether the resource folder exists. If it does not exist, create folders level by level.
        if (!checkResourcesFolder(remote_path_w)) {
            std::cout << "Failed to create remote resources folder: "
                      << std::string(remote_path_w.begin(), remote_path_w.end()) << std::endl;
            return false;
        }

        bool status = CopyFileW(local_path_w.c_str(), remote_path_w.c_str(), FALSE) != 0;

        std::string status_string;
        if (status) {
            status_string.assign("succeeded!");
        } else {
            status_string.assign("failed!");
        }

        std::cout << "SMB upload "
                  << "from \"" << std::string(remote_path_w.begin(), remote_path_w.end()) << "\" "
                  << "to \"" << local_path << "\"" << ", "
                  << status_string << std::endl;

        return status;
    }

    bool downloadFileKernel(const std::string &remote_path, const std::string &local_path) {
        if (remote_path.empty() || local_path.empty()) return false;

        std::wstring remote_path_w = string2wstring(remote_path);
        std::wstring local_path_w = string2wstring(local_path);

        // refactor remote path
        if (remote_path_w.front() == L'\\') remote_path_w = L"\\" + remote_path_w;
        remote_path_w = m_server_w + remote_path_w;

        // Determine whether the resource folder exists. If it does not exist, create folders level by level.
        if (!checkResourcesFolder(local_path_w)) {
            std::cout << "Failed to create local resources folder: "
                      << std::string(local_path_w.begin(), local_path_w.end()) << std::endl;
            return false;
        }

        bool status = CopyFileW(remote_path_w.c_str(), local_path_w.c_str(), FALSE) != 0;

        std::string status_string;
        if (status) {
            status_string.assign("succeeded!");
        } else {
            status_string.assign("failed!");
        }

        std::cout << "SMB download "
                  << "from \"" << std::string(remote_path_w.begin(), remote_path_w.end()) << "\" "
                  << "to \"" << local_path << "\"" << ", "
                  << status_string << std::endl;

        return status;
    }

    /**
     * @brief string 转 wstring
     *
     * @param str src string
     * @param CodePage The encoding format of the file calling this function. CP_ACP for gbk, CP_UTF8 for utf-8.
     * @return dst string
     */
    static std::wstring string2wstring(const std::string &str, _In_ UINT CodePage = CP_ACP) {
        int len = MultiByteToWideChar(CodePage, 0, str.c_str(), -1, nullptr, 0);
        std::wstring wstr(len, L'\0');
        MultiByteToWideChar(CodePage, 0, str.c_str(), -1, const_cast<wchar_t *>(wstr.data()), len);
        wstr.resize(wcslen(wstr.c_str()));
        return wstr;
    }

    /**
     * @brief wstring 转 string
     *
     * @param wstr src wstring
     * @param CodePage The encoding format of the file calling this function. CP_ACP for gbk, CP_UTF8 for utf-8.
     * @return dst string
     */
    static std::string wstring2string(const std::wstring &wstr, _In_ UINT CodePage = CP_ACP) {
        int len = WideCharToMultiByte(CodePage, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string str(len, '\0');
        WideCharToMultiByte(CodePage, 0, wstr.c_str(), -1, const_cast<char *>(str.data()), len, nullptr, nullptr);
        str.resize(strlen(str.c_str()));
        return str;
    }

private:
    std::wstring m_server_w;
    std::wstring m_username_w;
    std::wstring m_password_w;
};


#endif  // SMB_CLIENT_HPP
