### cite

[C++FTP客户端库：ftplibpp的使用-CSDN博客](https://blog.csdn.net/u014552102/article/details/116835678)

### source

https://github.com/mkulke/ftplibpp.git



### example


```cpp
#include <iostream>
#include <string>
#include <vector>
#include "ftplib.h"


class FtpClient {
public:
    FtpClient(const std::string &host, const std::string &username, const std::string &password)
        : m_host(host),
          m_username(username),
          m_password(password) {}

    FtpClient() = default;

    ~FtpClient() = default;

public:
    /**
     * @brief setParam
     *
     * @param host Example: "192.168.1.18:21"
     * @param username
     * @param password
     * @return
     */
    void setParam(const std::string &host, const std::string &username, const std::string &password) {
        m_host = host;
        m_username = username;
        m_password = password;
    }

    /**
     * @brief uploadFile
     *
     * @param local_path
     * @param remote_path R"(\xxx\xxx.txt)"
     * @return
     */
    bool uploadFile(const std::string &local_path, const std::string &remote_path) {
        if (connectToFtp()) {
            bool result = m_ftp.Put(local_path.c_str(), .c_str(), ftplib::image) != 0;
            
            std::cout << "Ftp upload: " << std::endl;
            std::cout << "  From " << local_path << std::endl;
            std::cout << "  To " << remote_path << std::endl;

            disconnect();
            return result;
        }

        std::cout << "Ftp connect failed!" << std::endl;
        
        return false;
    }

private:
    inline bool connectToFtp() {
        return ((m_ftp.Connect(m_host.c_str()) != 0) && (m_ftp.Login(m_username.c_str(), m_password.c_str()) != 0));
    }

    inline void disconnect() {
        m_ftp.Quit();
    }

private:
    ftplib m_ftp;

    std::string m_host;
    std::string m_username;
    std::string m_password;
};
```



### note

本目录下的代码已修改！

```cpp
// "ftplib.h" original
#ifndef _WIN32
#include <unistd.h>
#include <sys/time.h>
#endif

// "ftplib.h" after edited
#ifndef _WIN32
#include <unistd.h>
#include <sys/time.h>
#else
#include <WinSock2.h>
#include <ws2tcpip.h>
#endif

------------------------------------------------------------------------------------

// "ftplib.cpp" original
#ifndef NOSSL
	if (nControl->tlsdata)
	{
		(*nData)->ssl = SSL_new(nControl->ctx);
		(*nData)->sbio = BIO_new_socket((*nData)->handle, BIO_NOCLOSE);
		SSL_set_bio((*nData)->ssl,(*nData)->sbio,(*nData)->sbio);
		ret = SSL_connect((*nData)->ssl);
		if (ret != 1) return 0;
		(*nData)->tlsdata = 1;
	}
#endif

// "ftplib.cpp" after edited
#ifndef NOSSL
	if (nControl->tlsdata)
	{
		(*nData)->ssl = SSL_new(nControl->ctx);
		(*nData)->sbio = BIO_new_socket((*nData)->handle, BIO_NOCLOSE);
		SSL_set_bio((*nData)->ssl,(*nData)->sbio,(*nData)->sbio);
		int ret = SSL_connect((*nData)->ssl);
		if (ret != 1) return 0;
		(*nData)->tlsdata = 1;
	}
#endif

------------------------------------------------------------------------------------

// "ftplib.cpp" original
#if defined(_WIN32)
#include <windows.h>
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

// "ftplib.cpp" after edited
#if defined(_WIN32)
#include <windows.h>
#include <winsock.h>
#pragma comment(lib,"ws2_32")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif
```

