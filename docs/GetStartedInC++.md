# Getting Started with SessionAppFramework (C++)

This guide will help you build your first Session bot from scratch using the SAF library.

## 1. Prerequisites & Dependencies
Before building your project, you must manually build the `libsession-util` dependency.

```bash
# 1. Enter the submodule directory
cd lib/SessionAppFramework/libsession-util

# 2. Run the build script (this handles internal dependencies like sodium and protobuf)
./build.sh

# 3. Return to your project root
cd ../../..
```

## 2. Project Structure
Create a new folder for your project and set it up as follows:
```text
my-session-bot/
├── CMakeLists.txt
├── main.cpp
└── lib/
    └── SessionAppFramework/ (cloned here as a submodule)
```

## 3. CMake Configuration
Use this `CMakeLists.txt` to link against the framework. It handles the dependencies and includes for you.

```cmake
cmake_minimum_required(VERSION 3.16)
project(MySessionBot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Add the framework subdirectory
add_subdirectory(lib/SessionAppFramework)

# Create your bot executable
add_executable(MySessionBot main.cpp)

# Link against SAF (this also pulls in libsession-util, curl, etc.)
target_link_libraries(MySessionBot PRIVATE SAF::SessionAppFramework)
```

## 4. Your First Bot: Ping-Pong
Copy this code into `main.cpp`. This bot will reply "Pong!" to any message containing "ping" (in DMs or Groups).

```cpp
#include <SessionAppFramework/Session.hpp>
#include <iostream>
#include <csignal>

// Global flag to handle Ctrl+C
std::atomic<bool> g_Running{true};
void OnSignal(int) { g_Running = false; }

int main() {
    // 1. Setup Signal Handling
    std::signal(SIGINT, OnSignal);

    // 2. Initialize the Client
    // Pass a mnemonic string here to load an existing account, 
    // or leave empty to generate a brand new one.
    Session::Client client(""); 

    std::cout << "Starting Bot..." << std::endl;
    std::cout << "My Session ID: " << client.GetMe().GetId() << std::endl;
    
    // IMPORTANT: If you generated a new account, save this mnemonic!
    std::cout << "Mnemonic: " << client.GetMnemonic() << std::endl;

    // 3. Define Message Handling
    client.OnMessage([&](Session::Message msg) {
        // Ignore messages sent by the bot itself to avoid infinite loops
        if (msg.GetAuthor().GetId() == client.GetMe().GetId()) return;

        std::string content = msg.GetContent();
        std::cout << "Received: " << content << std::endl;

        // Simple logic: if text contains "ping" (case insensitive-ish)
        if (content.find("ping") != std::string::npos || content.find("Ping") != std::string::npos) {
            
            // The .Reply() method automatically detects if it's a DM or a Group
            msg.Reply("Pong! 🏓");
            
            // You can also react to the message
            msg.React("🔥");
        }
    });

    // 4. Start the Client (Non-blocking)
    client.Start();
    std::cout << "Bot is now online. Invite it to a group or send a DM!" << std::endl;

    // 5. Keep the program running until Ctrl+C
    while (g_Running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Shutting down..." << std::endl;
    client.Stop();
    return 0;
}
```

## 5. Build & Run

### Linux / macOS
```bash
bash build.sh
./Build/examples/MySessionBot
```

### Windows
```cmd
build.bat
.\Build\examples\Release\MySessionBot.exe
```

## Key Concepts for Beginners
- **Client**: Your connection to the Session network. Always call `.Start()` to begin receiving messages.
- **Message**: An object containing everything about a received event. You can call `.Reply()`, `.React()`, or `.GetGroup()` on it.
- **User**: Represents a Session account. You can send messages to a user via `client.GetUser("05...").SendMessage("Hi")`.
- **Group**: Represents a closed group. You can join them automatically (SAF handles invitations for you if you use `Session::Client`).
