#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <locale>
#include <vector>
#include <io.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

// 常量定义
const size_t CHUNK_SIZE = 64 * 1024; // 64KB 分块大小
const int DEFAULT_BUFFER_SIZE = 8192;

// 错误处理宏
#define CHECK_WINSOCK_ERROR(result, message) \
    if ((result) == SOCKET_ERROR) { \
        std::cerr << (message) << ": " << WSAGetLastError() << std::endl; \
        return false; \
    }

#define CHECK_WIN32_ERROR(result, message) \
    if ((result) == FALSE) { \
        std::cerr << (message) << ": " << GetLastError() << std::endl; \
        return false; \
    }

// 字节序转换函数
uint32_t htonl(uint32_t hostlong) {
    return ((hostlong & 0xFF) << 24) | 
           ((hostlong & 0xFF00) << 8) | 
           ((hostlong & 0xFF0000) >> 8) | 
           ((hostlong & 0xFF000000) >> 24);
}

uint64_t htonll(uint64_t hostlonglong) {
    return ((uint64_t)htonl((uint32_t)(hostlonglong & 0xFFFFFFFF)) << 32) | 
           htonl((uint32_t)(hostlonglong >> 32));
}

uint32_t ntohl(uint32_t netlong) {
    return htonl(netlong);
}

uint64_t ntohll(uint64_t netlonglong) {
    return htonll(netlonglong);
}

// 可靠的发送函数 - 解决粘包问题
bool send_all(SOCKET sock, const char* data, size_t length) {
    size_t total_sent = 0;
    while (total_sent < length) {
        int sent = send(sock, data + total_sent, (int)(length - total_sent), 0);
        if (sent == SOCKET_ERROR) {
            std::cerr << "send failed: " << WSAGetLastError() << std::endl;
            return false;
        }
        total_sent += sent;
    }
    return true;
}

// 可靠的接收函数 - 解决分包问题
bool recv_all(SOCKET sock, char* buffer, size_t length) {
    size_t total_received = 0;
    while (total_received < length) {
        int received = recv(sock, buffer + total_received, (int)(length - total_received), 0);
        if (received == SOCKET_ERROR) {
            std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
            return false;
        }
        if (received == 0) {
            std::cerr << "Connection closed by peer" << std::endl;
            return false;
        }
        total_received += received;
    }
    return true;
}

// 获取文件大小
uint64_t get_file_size(const std::string& file_path) {
    HANDLE hFile = CreateFileA(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ, 
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(hFile, &file_size)) {
        CloseHandle(hFile);
        return 0;
    }
    
    CloseHandle(hFile);
    return (uint64_t)file_size.QuadPart;
}

// 判断是否为目录
bool is_directory(const std::string& path) {
    DWORD attributes = GetFileAttributesA(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// 递归创建目录
bool ensure_directories_for_path(const std::string& full_path) {
    size_t pos = full_path.find_last_of("\\/");
    if (pos == std::string::npos) {
        return true;
    }
    std::string dir = full_path.substr(0, pos);
    if (dir.empty()) {
        return true;
    }

    // 逐级创建
    std::string current;
    for (size_t i = 0; i < dir.size(); ++i) {
        char c = dir[i];
        if (c == '\\' || c == '/') {
            if (!current.empty()) {
                CreateDirectoryA(current.c_str(), NULL);
            }
        }
        current.push_back(c);
    }
    if (!current.empty()) {
        CreateDirectoryA(current.c_str(), NULL);
    }
    return true;
}

// 递归收集目录中的所有文件，返回 pair<绝对路径, 相对路径>
void collect_files_recursively(const std::string& base_dir,
                               const std::string& relative_prefix,
                               std::vector<std::pair<std::string, std::string>>& files) {
    std::string search_path = base_dir + "\\*";
    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path.c_str(), &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        const char* name = find_data.cFileName;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }
        std::string abs_path = base_dir + "\\" + name;
        std::string rel_path = relative_prefix.empty() ? std::string(name) : (relative_prefix + "\\" + name);
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            collect_files_recursively(abs_path, rel_path, files);
        } else {
            files.emplace_back(abs_path, rel_path);
        }
    } while (FindNextFileA(hFind, &find_data));
    FindClose(hFind);
}

// 服务端功能
bool run_server(int port, const std::string& output_dir) {
    std::cout << "Starting server on port " << port << std::endl;
    std::cout << "Output directory: " << output_dir << std::endl;
    
    // 创建输出目录
    CreateDirectoryA(output_dir.c_str(), NULL);
    
    // 创建监听套接字
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        std::cerr << "Failed to create socket: " << WSAGetLastError() << std::endl;
        return false;
    }
    
    // 设置地址重用
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    
    // 绑定地址
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(listen_sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(listen_sock);
        return false;
    }
    
    // 开始监听
    if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(listen_sock);
        return false;
    }
    
    std::cout << "Server listening on port " << port << std::endl;
    
    while (true) {
        sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);
        SOCKET client_sock = accept(listen_sock, (sockaddr*)&client_addr, &client_addr_len);
        
        if (client_sock == INVALID_SOCKET) {
            std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
            continue;
        }
        
        std::cout << "Client connected from " << inet_ntoa(client_addr.sin_addr) << std::endl;
        
        // 接收文件名长度
        uint32_t filename_len;
        if (!recv_all(client_sock, (char*)&filename_len, sizeof(filename_len))) {
            std::cerr << "Failed to receive filename length" << std::endl;
            closesocket(client_sock);
            continue;
        }
        filename_len = ntohl(filename_len);
        
        // 接收文件大小
        uint64_t file_size;
        if (!recv_all(client_sock, (char*)&file_size, sizeof(file_size))) {
            std::cerr << "Failed to receive file size" << std::endl;
            closesocket(client_sock);
            continue;
        }
        file_size = ntohll(file_size);
        
        // 接收文件名
        std::vector<char> filename_buffer(filename_len + 1);
        if (!recv_all(client_sock, filename_buffer.data(), filename_len)) {
            std::cerr << "Failed to receive filename" << std::endl;
            closesocket(client_sock);
            continue;
        }
        filename_buffer[filename_len] = '\0';
        std::string filename(filename_buffer.data());
        
        std::cout << "Receiving file: " << filename << " (" << file_size << " bytes)" << std::endl;
        
        // 构建输出文件路径（支持子目录）
        std::string output_path = output_dir + "\\" + filename;
        ensure_directories_for_path(output_path);
        
        // 创建输出文件
        HANDLE hFile = CreateFileA(output_path.c_str(), GENERIC_WRITE, 0, NULL, 
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            std::cerr << "Failed to create output file: " << GetLastError() << std::endl;
            closesocket(client_sock);
            continue;
        }
        
        // 接收文件内容
        std::vector<char> buffer(CHUNK_SIZE);
        uint64_t total_received = 0;
        bool success = true;
        
        while (total_received < file_size) {
            size_t chunk_size = (file_size - total_received < CHUNK_SIZE) ? 
                               (size_t)(file_size - total_received) : CHUNK_SIZE;
            
            if (!recv_all(client_sock, buffer.data(), chunk_size)) {
                std::cerr << "Failed to receive file data" << std::endl;
                success = false;
                break;
            }
            
            DWORD bytes_written;
            if (!WriteFile(hFile, buffer.data(), (DWORD)chunk_size, &bytes_written, NULL)) {
                std::cerr << "Failed to write file: " << GetLastError() << std::endl;
                success = false;
                break;
            }
            
            total_received += chunk_size;
            std::cout << "\rProgress: " << (total_received * 100 / file_size) << "%" << std::flush;
        }
        
        CloseHandle(hFile);
        closesocket(client_sock);
        
        if (success) {
            std::cout << "\nFile received successfully: " << output_path << std::endl;
        } else {
            std::cout << "\nFile transfer failed" << std::endl;
            DeleteFileA(output_path.c_str());
        }
    }
    
    closesocket(listen_sock);
    return true;
}

// 客户端功能
bool send_one_file(const std::string& host, int port,
                   const std::string& absolute_path,
                   const std::string& relative_name) {
    std::cout << "Connecting to " << host << ":" << port << std::endl;
    std::cout << "Sending file: " << relative_name << " from " << absolute_path << std::endl;

    HANDLE hFile = CreateFileA(absolute_path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open file: " << GetLastError() << std::endl;
        return false;
    }

    LARGE_INTEGER file_size_large;
    if (!GetFileSizeEx(hFile, &file_size_large)) {
        std::cerr << "Failed to get file size: " << GetLastError() << std::endl;
        CloseHandle(hFile);
        return false;
    }
    uint64_t file_size = (uint64_t)file_size_large.QuadPart;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Failed to create socket: " << WSAGetLastError() << std::endl;
        CloseHandle(hFile);
        return false;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) != 1) {
        std::cerr << "Invalid IP address: " << host << std::endl;
        closesocket(sock);
        CloseHandle(hFile);
        return false;
    }

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Connect failed: " << WSAGetLastError() << std::endl;
        closesocket(sock);
        CloseHandle(hFile);
        return false;
    }

    uint32_t filename_len = (uint32_t)relative_name.length();
    uint32_t filename_len_net = htonl(filename_len);
    if (!send_all(sock, (char*)&filename_len_net, sizeof(filename_len_net))) {
        std::cerr << "Failed to send filename length" << std::endl;
        closesocket(sock);
        CloseHandle(hFile);
        return false;
    }

    uint64_t file_size_net = htonll(file_size);
    if (!send_all(sock, (char*)&file_size_net, sizeof(file_size_net))) {
        std::cerr << "Failed to send file size" << std::endl;
        closesocket(sock);
        CloseHandle(hFile);
        return false;
    }

    if (!send_all(sock, relative_name.c_str(), filename_len)) {
        std::cerr << "Failed to send filename" << std::endl;
        closesocket(sock);
        CloseHandle(hFile);
        return false;
    }

    std::vector<char> buffer(CHUNK_SIZE);
    uint64_t total_sent = 0;
    bool success = true;
    while (total_sent < file_size) {
        DWORD bytes_read;
        size_t chunk_size = (file_size - total_sent < CHUNK_SIZE) ?
                           (size_t)(file_size - total_sent) : CHUNK_SIZE;
        if (!ReadFile(hFile, buffer.data(), (DWORD)chunk_size, &bytes_read, NULL)) {
            std::cerr << "Failed to read file: " << GetLastError() << std::endl;
            success = false;
            break;
        }
        if (bytes_read == 0) {
            break;
        }
        if (!send_all(sock, buffer.data(), bytes_read)) {
            std::cerr << "Failed to send file data" << std::endl;
            success = false;
            break;
        }
        total_sent += bytes_read;
        std::cout << "\rProgress: " << (total_sent * 100 / file_size) << "%" << std::flush;
    }

    CloseHandle(hFile);
    closesocket(sock);
    std::cout << std::endl;
    return success;
}

bool run_client(const std::string& host, int port, const std::string& file_path) {
    if (is_directory(file_path)) {
        std::cout << "Directory detected, collecting files..." << std::endl;
        std::vector<std::pair<std::string, std::string>> files;
        // 标准化为不以分隔符结尾
        std::string base = file_path;
        while (!base.empty() && (base.back() == '\\' || base.back() == '/')) {
            base.pop_back();
        }
        collect_files_recursively(base, "", files);
        if (files.empty()) {
            std::cout << "No files to send." << std::endl;
            return true;
        }
        bool all_ok = true;
        for (const auto& p : files) {
            if (!send_one_file(host, port, p.first, p.second)) {
                all_ok = false;
                // 不中断，尽量发送剩余文件
            }
        }
        return all_ok;
    }

    // 单文件路径
    size_t last_slash = file_path.find_last_of("/\\");
    std::string filename = (last_slash != std::string::npos) ? file_path.substr(last_slash + 1) : file_path;
    return send_one_file(host, port, file_path, filename);
}

// 显示使用说明
void show_usage(const char* program_name) {
    std::cout << "Remote File Transfer Tool" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  Server: " << program_name << " server <port> <output_dir>" << std::endl;
    std::cout << "  Client: " << program_name << " client <host> <port> <path>" << std::endl;
    std::cout << "           <path> 可以是单个文件或目录（将递归发送目录内文件）" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << " server 8080 C:\\received_files" << std::endl;
    std::cout << "  " << program_name << " client 127.0.0.1 8080 C:\\test.txt" << std::endl;
    std::cout << "  " << program_name << " client 127.0.0.1 8080 C:\\my_folder" << std::endl;
}

// 获取可执行文件所在目录
std::string get_executable_dir() {
    char path[MAX_PATH] = {0};
    DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return ".";
    }
    std::string full(path);
    size_t pos = full.find_last_of("/\\");
    if (pos == std::string::npos) {
        return ".";
    }
    return full.substr(0, pos);
}

// UTF-8 文本安全输出到控制台（避免中文乱码）
void print_utf8(const std::string& text) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE || hOut == NULL) {
        std::cout << text; // 回退
        return;
    }
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), NULL, 0);
    if (wlen <= 0) {
        std::cout << text;
        return;
    }
    std::vector<wchar_t> wbuf((size_t)wlen);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), wbuf.data(), wlen);
    DWORD written = 0;
    WriteConsoleW(hOut, wbuf.data(), (DWORD)wbuf.size(), &written, NULL);
}

int main(int argc, char* argv[]) {
    // 初始化 Winsock
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        return 1;
    }
    
    // 设置控制台为 UTF-8，避免中文乱码
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    setlocale(LC_ALL, "");

    // 无参数：交互式选择
    if (argc < 2) {
        print_utf8("请选择模式: 1) 服务端  2) 客户端 [1/2]: ");
        std::string choice;
        std::getline(std::cin, choice);
        if (choice == "1" || choice == "server" || choice == "s") {
            int port = 8080;
            std::string output_dir = get_executable_dir();
            print_utf8("将以服务端启动。监听端口: ");
            std::cout << port;
            print_utf8(", 保存路径: ");
            std::cout << output_dir << std::endl;
            if (!run_server(port, output_dir)) {
                WSACleanup();
                return 1;
            }
            WSACleanup();
            return 0;
        } else if (choice == "2" || choice == "client" || choice == "c") {
            std::string host;
            std::string port_str;
            std::string path;
            print_utf8("请输入服务器地址 [默认 127.0.0.1]: ");
            std::getline(std::cin, host);
            if (host.empty()) host = "127.0.0.1";
            print_utf8("请输入端口 [默认 8080]: ");
            std::getline(std::cin, port_str);
            int port = port_str.empty() ? 8080 : atoi(port_str.c_str());
            if (port <= 0 || port > 65535) {
                print_utf8("端口无效\n");
                WSACleanup();
                return 1;
            }
            print_utf8("请输入要发送的文件或目录路径: ");
            std::getline(std::cin, path);
            if (path.empty()) {
                print_utf8("未提供路径\n");
                WSACleanup();
                return 1;
            }
            if (!run_client(host, port, path)) {
                WSACleanup();
                return 1;
            }
            WSACleanup();
            return 0;
        } else {
            print_utf8("无效选择\n");
            show_usage(argv[0]);
            WSACleanup();
            return 1;
        }
    }
    
    std::string mode = argv[1];
    
    if (mode == "server") {
        int port = 8080;
        std::string output_dir = get_executable_dir();
        if (argc == 4) {
            port = atoi(argv[2]);
            output_dir = argv[3];
        } else if (argc != 2) {
            std::cerr << "Server mode requires <port> <output_dir>，或仅 'server' 使用默认设置" << std::endl;
            show_usage(argv[0]);
            WSACleanup();
            return 1;
        }
        
        if (port <= 0 || port > 65535) {
            std::cerr << "Invalid port number: " << argv[2] << std::endl;
            WSACleanup();
            return 1;
        }
        
        if (!run_server(port, output_dir)) {
            WSACleanup();
            return 1;
        }
    }
    else if (mode == "client") {
        std::string host;
        int port = 8080;
        std::string file_path;
        if (argc == 5) {
            host = argv[2];
            port = atoi(argv[3]);
            file_path = argv[4];
        } else if (argc == 2) {
            print_utf8("请输入服务器地址 [默认 127.0.0.1]: ");
            std::getline(std::cin, host);
            if (host.empty()) host = "127.0.0.1";
            std::string port_str;
            print_utf8("请输入端口 [默认 8080]: ");
            std::getline(std::cin, port_str);
            if (!port_str.empty()) port = atoi(port_str.c_str());
            print_utf8("请输入要发送的文件或目录路径: ");
            std::getline(std::cin, file_path);
        } else {
            print_utf8("Client 模式需要 <host> <port> <path>，或仅 'client' 进入交互式\n");
            show_usage(argv[0]);
            WSACleanup();
            return 1;
        }
        
        if (port <= 0 || port > 65535) {
            std::cerr << "Invalid port number: " << argv[3] << std::endl;
            WSACleanup();
            return 1;
        }
        
        if (!run_client(host, port, file_path)) {
            WSACleanup();
            return 1;
        }
    }
    else {
        std::cerr << "Invalid mode: " << mode << std::endl;
        show_usage(argv[0]);
        WSACleanup();
        return 1;
    }
    
    WSACleanup();
    return 0;
}
