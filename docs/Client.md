# Session::Client

The `Client` class is the main entry point for the SessionAppFramework. It manages the account identity, network connections, and message polling.

## Constructor
- `Client(const std::string& seedOrMnemonic = "")`: Initializes the client. If no seed is provided, a brand new account is generated.

## Identity Methods
- `User GetMe()`: Returns a `User` object representing the local account.
- `std::string GetMnemonic()`: Returns the 25-word recovery phrase (mnemonic) or the 64-char hex seed.
- `void SetDisplayName(const std::string& name)`: Updates the account's display name on the swarm.
- `void SetProfilePicture(const std::string& filePath)`: Uploads and sets the account's profile picture.

## Configuration Methods
- `void SetMessageDbPath(const std::string& path)`: Changes the path where processed message IDs are stored (default: `"messages.db"`).
- `std::string GetMessageDbPath()`: Returns the current database path.

### About the Message Database
Session messages are stored on decentralized nodes (swarms). Since nodes may return the same message multiple times during polling, SAF uses a small local binary file (the Message DB) to keep track of which messages have already been processed. 
- If you delete this file, the bot will re-process old messages.
- If you run multiple bots, ensure they each use a different database path.

## Lifecycle Methods
- `void Start()`: Bootstraps the network, syncs configuration, and begins polling for messages.
- `void Stop()`: Stops polling and clears resources.

## Events
- `void OnMessage(MessageCallback callback)`: Registers a callback triggered for every incoming text or file message.
- `void OnReaction(ReactionCallback callback)`: Registers a callback triggered when someone adds or removes a reaction to a message.

## Factory Methods
- `Group CreateGroup(const std::string& name)`: Creates a new V2 closed group.
- `Group GetGroup(const std::string& groupId)`: Returns a `Group` object for the given ID.
- `User GetUser(const std::string& userId)`: Returns a `User` object for the given Session ID.
