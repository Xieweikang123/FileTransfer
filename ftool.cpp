#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
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
        
        // 构建输出文件路径
        std::string output_path = output_dir + "\\" + filename;
        
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
bool run_client(const std::string& host, int port, const std::string& file_path) {
    std::cout << "Connecting to " << host << ":" << port << std::endl;
    std::cout << "Sending file: " << file_path << std::endl;
    
    // 检查文件是否存在
    HANDLE hFile = CreateFileA(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ, 
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open file: " << GetLastError() << std::endl;
        return false;
    }
    
    // 获取文件大小
    LARGE_INTEGER file_size_large;
    if (!GetFileSizeEx(hFile, &file_size_large)) {
        std::cerr << "Failed to get file size: " << GetLastError() << std::endl;
        CloseHandle(hFile);
        return false;
    }
    uint64_t file_size = (uint64_t)file_size_large.QuadPart;
    
    // 获取文件名
    size_t last_slash = file_path.find_last_of("/\\");
    std::string filename = (last_slash != std::string::npos) ? 
                          file_path.substr(last_slash + 1) : file_path;
    
    // 创建套接字
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Failed to create socket: " << WSAGetLastError() << std::endl;
        CloseHandle(hFile);
        return false;
    }
    
    // 连接服务器
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
    
    std::cout << "Connected to server" << std::endl;
    
    // 发送文件名长度
    uint32_t filename_len = (uint32_t)filename.length();
    uint32_t filename_len_net = htonl(filename_len);
    if (!send_all(sock, (char*)&filename_len_net, sizeof(filename_len_net))) {
        std::cerr << "Failed to send filename length" << std::endl;
        closesocket(sock);
        CloseHandle(hFile);
        return false;
    }
    
    // 发送文件大小
    uint64_t file_size_net = htonll(file_size);
    if (!send_all(sock, (char*)&file_size_net, sizeof(file_size_net))) {
        std::cerr << "Failed to send file size" << std::endl;
        closesocket(sock);
        CloseHandle(hFile);
        return false;
    }
    
    // 发送文件名
    if (!send_all(sock, filename.c_str(), filename_len)) {
        std::cerr << "Failed to send filename" << std::endl;
        closesocket(sock);
        CloseHandle(hFile);
        return false;
    }
    
    // 发送文件内容
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
    
    if (success) {
        std::cout << "\nFile sent successfully" << std::endl;
        return true;
    } else {
        std::cout << "\nFile transfer failed" << std::endl;
        return false;
    }
}

// 显示使用说明
void show_usage(const char* program_name) {
    std::cout << "Remote File Transfer Tool" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  Server: " << program_name << " server <port> <output_dir>" << std::endl;
    std::cout << "  Client: " << program_name << " client <host> <port> <file_path>" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << " server 8080 C:\\received_files" << std::endl;
    std::cout << "  " << program_name << " client 127.0.0.1 8080 C:\\test.txt" << std::endl;
}

int main(int argc, char* argv[]) {
    // 初始化 Winsock
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        return 1;
    }
    
    // 检查参数
    if (argc < 2) {
        show_usage(argv[0]);
        WSACleanup();
        return 1;
    }
    
    std::string mode = argv[1];
    
    if (mode == "server") {
        if (argc != 4) {
            std::cerr << "Server mode requires port and output directory" << std::endl;
            show_usage(argv[0]);
            WSACleanup();
            return 1;
        }
        
        int port = atoi(argv[2]);
        std::string output_dir = argv[3];
        
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
        if (argc != 5) {
            std::cerr << "Client mode requires host, port, and file path" << std::endl;
            show_usage(argv[0]);
            WSACleanup();
            return 1;
        }
        
        std::string host = argv[2];
        int port = atoi(argv[3]);
        std::string file_path = argv[4];
        
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
