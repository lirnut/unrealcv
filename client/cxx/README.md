# UnrealCV C++ Client

C++17 implementation of the UnrealCV client library for Linux and Windows.

## Features

- TCP socket communication with UnrealCV server
- Binary message framing compatible with Python client
- Thread-safe request/response handling
- Synchronous and asynchronous APIs
- Batch request support
- Cross-platform (Windows/Linux)

## API

### Connection

```cpp
Client client({{"localhost", 9000}});
if (client.connect()) {
    // connected
}
client.disconnect();
```

### Single Request (Synchronous)

```cpp
auto response = client.request("vget /camera/0/location");
if (response) {
    std::cout << response.value() << std::endl;
}
```

### Batch Request (Synchronous)

```cpp
std::vector<std::string> commands = {
    "vget /camera/0/location",
    "vget /camera/0/rotation"
};
auto responses = client.request_batch(commands);
for (const auto& resp : responses) {
    if (resp) {
        std::cout << resp.value() << std::endl;
    }
}
```

### Async Request (No Response)

```cpp
client.request_async("vset /camera/0/location 100 100 100");

// For batch async
std::vector<std::string> commands = {"cmd1", "cmd2"};
client.request_batch_async(commands);
```

## Binary Format

The message format is identical to the Python implementation:

```
[Magic: uint32][PayloadSize: uint32][MessageID:Command]
```

- **Magic**: 0x9E2B83C1
- **PayloadSize**: uint32 size of payload in bytes
- **MessageID**: uint32 message counter, sent by client, echoed by server
- **Command**: UTF-8 command string

## Building

### Linux

```bash
mkdir build && cd build
cmake ..
make
```

### Windows

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

## Example Usage

See `example.cpp` for a complete usage example.

## Implementation Notes

- Uses `std::optional` for nullable return values
- Thread-safe message queuing with mutexes and condition variables
- Message ID tracking ensures request/response matching
- Automatic reconnection in Python implementation recommended for C++ wrapper
- Binary payload fully compatible with UE5 server implementation
