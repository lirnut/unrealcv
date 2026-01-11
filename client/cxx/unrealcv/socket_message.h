#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
#include <iostream>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace unrealcv {

class SocketMessage {
public:
    static constexpr uint32_t MAGIC = 0x9E2B83C1;

    static bool ReceivePayload(int sock, std::vector<uint8_t>& payload) noexcept {
        payload.clear();

        uint8_t raw_magic[4];
        if (!ReceiveExact(sock, raw_magic, 4)) {
            return false;
        }

        uint32_t magic = *reinterpret_cast<uint32_t*>(raw_magic);
        if (magic != MAGIC) {
            return false;
        }

        uint8_t raw_payload_size[4];
        if (!ReceiveExact(sock, raw_payload_size, 4)) {
            return false;
        }

        uint32_t payload_size = *reinterpret_cast<uint32_t*>(raw_payload_size);
        payload.resize(payload_size);

        if (!ReceiveExact(sock, payload.data(), payload_size)) {
            payload.clear();
            return false;
        }

        return true;
    }

    static bool WrapAndSendPayload(int sock, const std::vector<uint8_t>& payload) noexcept {
        try {
            uint32_t magic = MAGIC;
            uint32_t payload_size = static_cast<uint32_t>(payload.size());

            if (!SendExact(sock, reinterpret_cast<const uint8_t*>(&magic), 4)) {
                return false;
            }

            if (!SendExact(sock, reinterpret_cast<const uint8_t*>(&payload_size), 4)) {
                return false;
            }

            if (!SendExact(sock, payload.data(), payload.size())) {
                return false;
            }

            return true;
        } catch (const std::exception& e) {
            std::cerr << "[SocketMessage] Exception in WrapAndSendPayload: " << e.what() << std::endl;
            return false;
        } catch (...) {
            std::cerr << "[SocketMessage] Unknown exception in WrapAndSendPayload" << std::endl;
            return false;
        }
    }

    static bool WrapAndSendPayload(int sock, const std::string& payload) noexcept {
        const auto* data = reinterpret_cast<const uint8_t*>(payload.data());
        const std::vector<uint8_t> payload_vec(data, data + payload.size());
        return WrapAndSendPayload(sock, payload_vec);
    }

private:
    static bool ReceiveExact(int sock, uint8_t* buffer, size_t size) noexcept {
        size_t remain = size;
        while (remain > 0) {
            int received = recv(sock, reinterpret_cast<char*>(buffer + size - remain), static_cast<int>(remain), 0);
            if (received <= 0) {
                return false;
            }
            remain -= static_cast<size_t>(received);
        }
        return true;
    }

    static bool SendExact(int sock, const uint8_t* buffer, size_t size) noexcept {
        size_t remain = size;
        while (remain > 0) {
            int sent = send(sock, reinterpret_cast<const char*>(buffer + size - remain), static_cast<int>(remain), 0);
            if (sent <= 0) {
                return false;
            }
            remain -= static_cast<size_t>(sent);
        }
        return true;
    }
};

}
