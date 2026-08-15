/**
 * TcpServer.cpp — Educational TCP server implementation
 * 
 * OS CONCEPTS DEMONSTRATED:
 *   - Socket creation (socket syscall)
 *   - Binding to an address (bind syscall)
 *   - Listening for connections (listen syscall)
 *   - Accepting connections (accept syscall — blocking I/O)
 *   - Thread-per-connection model
 *   - Message framing (length-prefixed protocol)
 *   - Graceful shutdown via signal handling
 * 
 * TCP FUNDAMENTALS:
 *   TCP (Transmission Control Protocol) provides:
 *   1. Reliable delivery — retransmits lost packets
 *   2. Ordered delivery — packets arrive in order
 *   3. Error checking — checksums detect corruption
 *   4. Flow control — prevents overwhelming the receiver
 *   5. Connection-oriented — 3-way handshake before data transfer
 * 
 * SOCKET LIFECYCLE:
 *   Server: socket() → bind() → listen() → accept() → read/write → close()
 *   Client: socket() → connect() → read/write → close()
 */

#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <atomic>
#include <csignal>

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
    #include <unistd.h>
    #include <arpa/inet.h>
    using socket_t = int;
    #define CLOSE_SOCKET close
    #define INVALID_SOCK (-1)
#endif

static std::atomic<bool> g_running{true};

void signal_handler(int) { g_running = false; }

/**
 * Send a length-prefixed message.
 * 
 * MESSAGE FRAMING:
 *   TCP is a BYTE STREAM protocol — there are no message boundaries.
 *   Without framing, the receiver doesn't know where one message ends
 *   and the next begins. The 4-byte length prefix solves this:
 *   
 *   [4 bytes: message length in network byte order][message bytes]
 *   
 *   The receiver reads exactly 4 bytes to get the length, then reads
 *   exactly that many bytes for the message.
 */
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

/**
 * Handle a single client connection.
 * Each client runs in its own thread — this is the THREAD-PER-CONNECTION model.
 * 
 * Advantages: Simple, each client is independent
 * Disadvantages: Thread creation overhead, no upper bound on thread count
 * 
 * The main application uses a THREAD POOL instead, which bounds resource usage.
 */
void handle_client(socket_t client_sock, int client_id) {
    std::cout << "[Server] Client " << client_id << " connected\n";
    
    send_message(client_sock, "Welcome to the TCP Demo Server! Send messages, or 'quit' to exit.");
    
    while (g_running) {
        std::string msg = receive_message(client_sock);
        if (msg.empty() || msg == "quit") break;
        
        std::cout << "[Server] Client " << client_id << ": " << msg << "\n";
        
        std::string response = "Echo: " + msg + " (length=" + std::to_string(msg.size()) + ")";
        send_message(client_sock, response);
    }
    
    std::cout << "[Server] Client " << client_id << " disconnected\n";
    CLOSE_SOCKET(client_sock);
}

int main() {
    std::signal(SIGINT, signal_handler);
    
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    const int PORT = 9090;
    
    // STEP 1: Create socket
    // AF_INET = IPv4, SOCK_STREAM = TCP (reliable byte stream)
    socket_t server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == INVALID_SOCK) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }
    
    // Allow address reuse (prevents "Address already in use" on restart)
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, 
               reinterpret_cast<const char*>(&opt), sizeof(opt));
    
    // STEP 2: Bind socket to address
    // Associates the socket with a specific IP address and port number
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
    addr.sin_port = htons(PORT);         // Convert to network byte order (big-endian)
    
    if (bind(server_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Bind failed\n";
        CLOSE_SOCKET(server_sock);
        return 1;
    }
    
    // STEP 3: Listen for connections
    // The backlog (5) is the max number of pending connections in the queue
    if (listen(server_sock, 5) < 0) {
        std::cerr << "Listen failed\n";
        CLOSE_SOCKET(server_sock);
        return 1;
    }
    
    std::cout << "[TCP Demo Server] Listening on port " << PORT << "\n";
    std::cout << "Press Ctrl+C to stop\n\n";
    
    int client_count = 0;
    std::vector<std::thread> threads;
    
    while (g_running) {
        // STEP 4: Accept connections (BLOCKS until a client connects)
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        socket_t client_sock = accept(server_sock, 
                                       reinterpret_cast<sockaddr*>(&client_addr), 
                                       &client_len);
        
        if (client_sock == INVALID_SOCK) {
            if (g_running) std::cerr << "Accept failed\n";
            continue;
        }
        
        // Spawn a thread for this client
        threads.emplace_back(handle_client, client_sock, ++client_count);
    }
    
    // Graceful shutdown: join all client threads
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
    
    CLOSE_SOCKET(server_sock);
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    std::cout << "\n[Server] Shutdown complete\n";
    return 0;
}
