# SessionAppFramework (SAF)

A high-level C++ library for interacting with the Session network. Built on top of `libsession-util`.

## ⚠️ Important Warning
**It is strictly FORBIDDEN to use this framework to damage, spam, or otherwise maliciousy target the Session network or its users.** This tool is intended for building helpful bots and integrations.

## Capabilities
- **Account Management**: Create new accounts or load existing ones via mnemonic.
- **Identity**: Set display names and profile pictures.
- **Messaging**: Send and receive Direct Messages (DMs) and Group messages.
- **Interactions**: Full support for replies and reactions (emoji).
- **Group Management**: Create groups, add/remove members, and manage admin roles.
- **File Transfer**: End-to-end encrypted file uploads and downloads.
- **Dual Protocol**: Compatible with both Legacy (V1) and New Style (V2) closed groups.

## Simple Example
```cpp
#include <SessionAppFramework/Session.hpp>

int main() {
    Session::Client client("your_seed_here");
    
    client.OnMessage([](Session::Message msg) {
        if (msg.GetContent() == "/ping") {
            msg.Reply("Pong!");
        }
    });

    client.Start();
    // ... wait for signal ...
}
```

## Documentation
Detailed documentation for the high-level API can be found in the `/docs` folder:
- [🚀 Getting Started (C++)](./docs/GetStartedInC++.md)
- [Client](./docs/Client.md)
- [User](./docs/User.md)
- [Group](./docs/Group.md)
- [Message](./docs/Message.md)
