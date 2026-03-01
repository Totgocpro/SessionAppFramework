# Session::User

Represents a Session user (either the local account or a contact).

## Methods
- `std::string GetId()`: Returns the full Session ID (starting with `05`).
- `std::string GetDisplayName()`: Returns the best available name for the user.
- `void SendMessage(const std::string& text)`: Sends a private message (DM) to this user.
- `void SendFile(const std::string& filePath)`: Sends an encrypted file to this user.
