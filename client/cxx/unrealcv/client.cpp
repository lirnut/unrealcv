#include "client.h"
#include <iostream>
#include <chrono>
#include <algorithm>

#ifdef _WIN32
    #pragma comment(lib, "ws2_32.lib")
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

namespace unrealcv {

bool Client::initialize_socket_library() {
#ifdef _WIN32
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    return result == 0;
#else
    return true;
#endif
}

bool Client::cleanup_socket_library() {
#ifdef _WIN32
    WSACleanup();
#endif
    return true;
}

Client::Client(const Endpoint& endpoint)
    : endpoint_(endpoint),
      sock_(INVALID_SOCKET),
      send_message_id_(0),
      recv_message_id_(0),
      receive_thread_running_(false) {
    initialize_socket_library();
}

Client::~Client() {
    disconnect();
    cleanup_socket_library();
}

bool Client::connect(int timeout_sec) {
    if (is_connected()) {
        return true;
    }

    if (!connect_internal(timeout_sec)) {
        disconnect();
        return false;
    }

    std::vector<uint8_t> message;
    if (!SocketMessage::ReceivePayload(sock_, message)) {
        disconnect();
        return false;
    }

    std::string message_str(message.begin(), message.end());
    if (message_str.find("connected") == std::string::npos) {
        disconnect();
        return false;
    }

    receive_thread_running_ = true;
    receive_thread_ = std::thread(&Client::receive_loop_queue, this);

    return true;
}

bool Client::connect_internal(int timeout_sec) {
    sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_ == INVALID_SOCKET) {
        return false;
    }

#ifdef _WIN32
    DWORD timeout_ms = timeout_sec * 1000;
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    setsockopt(sock_, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
    struct timeval tv;
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(endpoint_.second);

#ifdef _WIN32
    addr.sin_addr.s_addr = inet_addr(endpoint_.first.c_str());
#else
    inet_pton(AF_INET, endpoint_.first.c_str(), &addr.sin_addr);
#endif

    if (::connect(sock_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        return false;
    }

    return true;
}

void Client::disconnect() {
    receive_thread_running_ = false;

    if (sock_ != INVALID_SOCKET) {
#ifdef _WIN32
        shutdown(sock_, SD_RECEIVE);
        closesocket(sock_);
#else
        shutdown(sock_, SHUT_RD);
        close(sock_);
#endif
        sock_ = INVALID_SOCKET;
    }

    if (receive_thread_.joinable()) {
        recv_num_cv_.notify_one();
        receive_thread_.join();
    }
}

bool Client::is_connected() const {
    return sock_ != INVALID_SOCKET;
}

std::optional<MessageWithId> Client::receive_message() {
    if (!is_connected()) {
        return std::nullopt;
    }

    std::vector<uint8_t> message;
    if (!SocketMessage::ReceivePayload(sock_, message)) {
        return std::nullopt;
    }

    auto handler_result = raw_message_handler(message);
    if (!handler_result) {
        return std::nullopt;
    }

    return MessageWithId{recv_message_id_.load(), handler_result.value()};
}

std::optional<std::string> Client::raw_message_handler(const std::vector<uint8_t>& raw_message) {
    static const std::regex message_pattern(R"(^(\d+):)");

    std::string message_str(raw_message.begin(), raw_message.end());
    std::smatch match;

    if (!std::regex_search(message_str, match, message_pattern)) {
        return std::nullopt;
    }

    uint32_t message_id = static_cast<uint32_t>(std::stoul(match[1].str()));

    if (message_id != recv_message_id_.load()) {
        return std::nullopt;
    }

    size_t colon_pos = message_str.find(':');
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }

    std::string message_body = message_str.substr(colon_pos + 1);
    return message_body;
}

void Client::receive_loop_queue() {
    while (receive_thread_running_) {
        std::unique_lock<std::mutex> lock(recv_num_mutex_);
        recv_num_cv_.wait(lock, [this] { return !recv_num_q_.empty() || !receive_thread_running_; });

        if (!receive_thread_running_) {
            break;
        }

        if (recv_num_q_.empty()) {
            continue;
        }

        int num = recv_num_q_.front();
        recv_num_q_.pop();
        lock.unlock();

        if (num < 0) {
            // need results
            for (int i = 0; i < -num; ++i) {
                auto msg_with_id = receive_message();
                if (msg_with_id) {
                    {
                        std::lock_guard<std::mutex> data_lock(recv_data_mutex_);
                        recv_data_q_.push(msg_with_id->body);
                    }
                    recv_data_cv_.notify_one();
                } else {
                    {
                        std::lock_guard<std::mutex> data_lock(recv_data_mutex_);
                        recv_data_q_.push(std::nullopt);
                    }
                    recv_data_cv_.notify_one();
                    break;
                }
                recv_message_id_++;
            }
        } else {
            // do not need results
            for (int i = 0; i < num; ++i) {
                receive_message();
                recv_message_id_++;
            }
        }
    }
}

std::vector<uint8_t> Client::format_message(uint32_t message_id, const std::string& message) {
    std::string formatted = std::to_string(message_id) + ":" + message;
    const auto* data = reinterpret_cast<const uint8_t*>(formatted.data());
    return std::vector<uint8_t>(data, data + formatted.size());
}

std::optional<std::string> Client::request(const std::string& message, int timeout_sec) {
    if (!is_connected()) {
        return std::nullopt;
    }

    uint32_t current_id = send_message_id_++;
    auto formatted = format_message(current_id, message);

    if (!SocketMessage::WrapAndSendPayload(sock_, formatted)) {
        return std::nullopt;
    }

    {
        std::lock_guard<std::mutex> lock(recv_num_mutex_);
        recv_num_q_.push(-1);
    }
    recv_num_cv_.notify_one();

    std::unique_lock<std::mutex> lock(recv_data_mutex_);
    if (!recv_data_cv_.wait_for(lock, std::chrono::seconds(timeout_sec),
                                [this] { return !recv_data_q_.empty(); })) {
        return std::nullopt;
    }

    if (recv_data_q_.empty()) {
        return std::nullopt;
    }

    auto result = recv_data_q_.front();
    recv_data_q_.pop();

    return result;
}

std::vector<std::optional<std::string>> Client::request_batch(const std::vector<std::string>& batch) {
    std::vector<std::optional<std::string>> results;

    if (!is_connected()) {
        for (size_t i = 0; i < batch.size(); ++i) {
            results.push_back(std::nullopt);
        }
        return results;
    }

    for (const auto& message : batch) {
        uint32_t current_id = send_message_id_++;
        auto formatted = format_message(current_id, message);

        if (!SocketMessage::WrapAndSendPayload(sock_, formatted)) {
            for (size_t i = results.size(); i < batch.size(); ++i) {
                results.push_back(std::nullopt);
            }
            return results;
        }
    }

    {
        std::lock_guard<std::mutex> lock(recv_num_mutex_);
        recv_num_q_.push(-static_cast<int>(batch.size()));
    }
    recv_num_cv_.notify_one();

    for (size_t i = 0; i < batch.size(); ++i) {
        std::unique_lock<std::mutex> lock(recv_data_mutex_);
        if (!recv_data_cv_.wait_for(lock, std::chrono::seconds(5),
                                    [this] { return !recv_data_q_.empty(); })) {
            results.push_back(std::nullopt);
            continue;
        }

        if (recv_data_q_.empty()) {
            results.push_back(std::nullopt);
            continue;
        }

        auto result = recv_data_q_.front();
        recv_data_q_.pop();
        results.push_back(result);
    }

    return results;
}

bool Client::request_async(const std::string& message) {
    if (!is_connected()) {
        return false;
    }

    uint32_t current_id = send_message_id_++;
    auto formatted = format_message(current_id, message);

    if (!SocketMessage::WrapAndSendPayload(sock_, formatted)) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(recv_num_mutex_);
        recv_num_q_.push(1);
    }
    recv_num_cv_.notify_one();

    return true;
}

bool Client::request_batch_async(const std::vector<std::string>& batch) {
    if (!is_connected()) {
        return false;
    }

    for (const auto& message : batch) {
        uint32_t current_id = send_message_id_++;
        auto formatted = format_message(current_id, message);

        if (!SocketMessage::WrapAndSendPayload(sock_, formatted)) {
            return false;
        }
    }

    {
        std::lock_guard<std::mutex> lock(recv_num_mutex_);
        recv_num_q_.push(static_cast<int>(batch.size()));
    }
    recv_num_cv_.notify_one();

    return true;
}

}
