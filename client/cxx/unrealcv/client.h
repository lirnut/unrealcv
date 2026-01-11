#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <regex>
#include <atomic>
#include "socket_message.h"

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

namespace unrealcv {

struct MessageWithId {
    uint32_t id;
    std::string body;
};

class Client {
public:
    using Endpoint = std::pair<std::string, uint16_t>;

    explicit Client(const Endpoint& endpoint);
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    bool connect(int timeout_sec = 1);
    void disconnect();
    bool is_connected() const;

    std::optional<std::string> request(const std::string& message, int timeout_sec = 5);

    std::vector<std::optional<std::string>> request_batch(
        const std::vector<std::string>& batch);

    bool request_async(const std::string& message);
    bool request_batch_async(const std::vector<std::string>& batch);

private:

    Endpoint endpoint_;
    int sock_;
    std::atomic<uint32_t> send_message_id_;
    std::atomic<uint32_t> recv_message_id_;

    std::queue<std::optional<std::string>> recv_data_q_;
    std::queue<int> recv_num_q_;
    std::mutex recv_data_mutex_;
    std::mutex recv_num_mutex_;
    std::condition_variable recv_data_cv_;
    std::condition_variable recv_num_cv_;

    std::thread receive_thread_;
    std::atomic<bool> receive_thread_running_;

    static bool initialize_socket_library();
    static bool cleanup_socket_library();

    bool connect_internal(int timeout_sec);
    std::optional<MessageWithId> receive_message();
    void receive_loop_queue();

    std::optional<std::string> raw_message_handler(const std::vector<uint8_t>& raw_message);
    static std::vector<uint8_t> format_message(uint32_t message_id, const std::string& message);
};

}
