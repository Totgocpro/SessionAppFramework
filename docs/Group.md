# Session::Group

Represents a Session closed group. Supports both V1 (Legacy) and V2 protocols.

## Metadata Methods
- `std::string GetId()`: Returns the Group ID (starting with `03` or `05`).
- `std::string GetName()`: Returns the group's name.
- `std::string GetDescription()`: Returns the group's description (V2 only).
- `bool IsAdmin()`: Returns true if the local account has admin privileges in this group.
- `std::vector<User> GetMembers()`: Returns the list of current group members.

## Action Methods
- `void SendMessage(const std::string& text)`: Sends a message to the group.
- `void SendFile(const std::string& filePath)`: Sends a file to the group.
- `void Leave()`: Leaves the group.

## Management Methods (Admin Only)
- `void SetName(const std::string& name)`: Changes the group name.
- `void AddMember(const std::string& userId)`: Invites a new user to the group.
- `void RemoveMember(const std::string& userId)`: Kicks a user from the group.
- `void PromoteMember(const std::string& userId)`: Grants admin rights to a member.
- `void DemoteMember(const std::string& userId)`: Removes admin rights from a member.
