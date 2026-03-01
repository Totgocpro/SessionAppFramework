# Session::Message

Represents a message received from the network.

## Properties
- `std::string GetId()`: Returns the message unique hash/ID.
- `std::string GetContent()`: Returns the text body of the message.
- `User GetAuthor()`: Returns the `User` who sent the message.
- `bool IsGroup()`: Returns true if the message was sent in a group.
- `Group GetGroup()`: Returns the `Group` object (throws if `IsGroup()` is false).

## File Methods
- `bool HasFile()`: Returns true if the message contains an attachment.
- `std::string GetFileName()`: Returns the original name of the attachment.
- `long GetFileSize()`: Returns the size of the file in bytes.
- `void SaveFile(const std::string& destPath)`: Downloads and decrypts the file to the specified path.

## Interaction Methods
- `void Reply(const std::string& text)`: Sends a reply to this message.
- `void React(const std::string& emoji)`: Adds a reaction to this message.
- `void MarkAsRead()`: Marks the message as read (Read Receipt).
