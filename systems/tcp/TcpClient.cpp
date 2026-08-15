/**
 * TcpClient.cpp — Educational TCP client implementation
 * 
 * Connects to the TcpServer demo and exchanges length-prefixed messages.
 */

#include <iostream>
#include <string>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socket_t = SOCKET;
    #define CLOSE_SOCKET closesocket
    #define INVALID_SOCK INVALID_SOCKET
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    using socket_t = int;
    #define CLOSE_SOCKET close
    #define INVALID_SOCK (-1)
#endif

void send_message(socket_t sock, const std::string& msg) {
    uint32_t len = htonl(static_cast<uint32_t>(msg.size()));
    send(sock, reinterpret_cast<const char*>(&len), 4, 0);
    send(sock, msg.c_str(), static_cast<int>(msg.size()), 0);
}

std::string receive_message(socket_t sock) {
    uint32_t len_net;
    int received = recv(sock, reinterpret_cast<char*>(&len_net), 4, 0);
    if (received <= 0) return "";
    uint32_t len = ntohl(len_net);
    std::string buffer(len, '\0');
    int total = 0;
    while (total < static_cast<int>(len)) {
        int n = recv(sock, buffer.data() + total, static_cast<int>(len) - total, 0);
        if (n <= 0) return "";
        total += n;
    }
    return buffer;
}

int main() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCK) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9090);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed\n";
        CLOSE_SOCKET(sock);
        return 1;
    }

    // Receive welcome message
    std::string welcome = receive_message(sock);
    std::cout << "[Server] " << welcome << "\n\n";

    std::string input;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        if (input.empty()) continue;

        send_message(sock, input);
        if (input == "quit") break;

        std::string response = receive_message(sock);
        if (response.empty()) {
            std::cout << "[Disconnected]\n";
            break;
        }
        std::cout << "[Server] " << response << "\n";
    }

    CLOSE_SOCKET(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
