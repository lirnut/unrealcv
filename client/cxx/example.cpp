#include <unrealcv/client.h>
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>

std::atomic<bool> exit_requested(false);

void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\n\nReceived Ctrl+C, disconnecting..." << std::endl;
        exit_requested = true;
    }
}

int main() {
    using namespace unrealcv;

    std::signal(SIGINT, signal_handler);

    uint16_t port;
    std::cout << "=== UnrealCV Interactive Client ===" << std::endl;
    std::cout << "Enter port number (default 9000): ";

    std::string input;
    std::getline(std::cin, input);

    if (input.empty()) {
        port = 9000;
    } else {
        try {
            port = static_cast<uint16_t>(std::stoi(input));
        } catch (...) {
            std::cerr << "Invalid port number" << std::endl;
            return 1;
        }
    }

    Client client(std::make_pair("localhost", port));

    std::cout << "Connecting to localhost:" << port << "..." << std::endl;
    if (!client.connect()) {
        std::cerr << "Connection failed!" << std::endl;
        return 1;
    }

    std::cout << "✓ Connected successfully!" << std::endl;
    std::cout << "\nEnter commands (or 'quit' to exit):" << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  vget /camera/0/location" << std::endl;
    std::cout << "  vget /camera/0/rotation" << std::endl;
    std::cout << "  vset /camera/0/location 100 100 100" << std::endl;
    std::cout << "  vget /object/list" << std::endl;
    std::cout << std::string(40, '-') << std::endl;

    while (!exit_requested) {
        std::cout << "> ";
        std::cout.flush();

        std::string command;
        if (!std::getline(std::cin, command)) {
            break;
        }

        if (exit_requested) {
            break;
        }

        if (command.empty()) {
            continue;
        }

        if (command == "quit" || command == "exit") {
            break;
        }

        if (!client.is_connected()) {
            std::cerr << "Error: Not connected" << std::endl;
            continue;
        }

        auto response = client.request(command);
        if (response) {
            std::cout << "< " << response.value() << std::endl;
        } else {
            std::cerr << "Error: Request failed or timeout" << std::endl;
        }
    }

    std::cout << "Disconnecting..." << std::endl;
    client.disconnect();
    std::cout << "Done!" << std::endl;

    return 0;
}
