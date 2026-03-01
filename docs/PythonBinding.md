# SessionAppFramework - Python Bindings

The `session_saf` module provides a high-level, Pythonic interface to the Session network.

## Installation
Currently, the bindings must be built from source using CMake.

```bash
mkdir Build && cd Build
cmake .. -DSAF_BUILD_PYTHON=ON
cmake --build .
```
This will produce a `session_saf.so` (Linux) or `session_saf.pyd` (Windows) file in your build directory.

## Getting Started: Ping-Pong Bot

```python
import session_saf
import time
import signal
import sys

# 1. Initialize Client (load seed or create new)
client = session_saf.Client("your_mnemonic_here_or_empty")

print(f"Bot ID: {client.get_me().get_id()}")

# 2. Configure (Optional)
client.set_display_name("Python Bot")
client.set_message_db_path("python_bot.db")

# 3. Define Handlers
def on_message(msg):
    # Ignore self
    if msg.get_author().get_id() == client.get_me().get_id():
        return

    content = msg.get_content()
    print(f"Received: {content}")

    if "ping" in content.lower():
        msg.reply("Pong! 🐍")
        msg.react("🔥")

def on_reaction(reactor, target, emoji, added):
    action = "added" if added else "removed"
    print(f"Reaction {action}: {emoji} by {reactor.get_id()}")

client.on_message(on_message)
client.on_reaction(on_reaction)

# 4. Start
client.start()
print("Bot is online. Press Ctrl+C to stop.")

def signal_handler(sig, frame):
    client.stop()
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)

# Keep main thread alive
while True:
    time.sleep(1)
```

## API Mapping (C++ to Python)

| C++ (PascalCase) | Python (snake_case) |
| --- | --- |
| `client.Start()` | `client.start()` |
| `client.GetMe()` | `client.get_me()` |
| `msg.GetContent()` | `msg.get_content()` |
| `msg.Reply(text)` | `msg.reply(text)` |
| `group.AddMember(id)` | `group.add_member(id)` |
| ... | ... |

*Note: All logical types (User, Group, Message) are mapped to Python classes.*
