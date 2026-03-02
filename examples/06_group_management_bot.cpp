/**
 * Example 06 – Group Management Bot
 * ─────────────────────────────────────────────────────────
 * This bot automatically accepts group invitations and supports
 * group commands like /send-file, /delete_member, and /members.
 */

#include <SessionAppFramework/Session.hpp>
#include <iostream>
#include <sstream>
#include <csignal>
#include <atomic>
#include <thread>

static std::atomic<bool> g_Running{true};
void SigHandler(int) { 
    g_Running = false;
}

void HandleCommand(Session::Client& client, Session::Message& msg) {
    std::string body = msg.GetContent();
    std::istringstream ss(body);
    std::string cmd; ss >> cmd;

    std::string senderId = msg.GetAuthor().GetId();

    if (cmd == "/help") {
        std::string help = "Commands:\n"
                           "/id - Show conversation ID\n"
                           "/members - List group members\n"
                           "/send-file <path> - Send a file\n"
                           "/react <emoji> - React to this command\n"
                           "/delete_member <id> - Kick a member (Admin only)";
        msg.Reply(help);
    }
    else if (cmd == "/id") {
        std::string dest = msg.IsGroup() ? msg.GetGroup().GetId() : senderId;
        msg.Reply("Conversation ID: " + dest);
    }
    else if (cmd == "/members") {
        if (!msg.IsGroup()) {
            msg.Reply("This is not a group.");
            return;
        }
        try {
            auto members = msg.GetGroup().GetMembers();
            std::string list = "Members (" + std::to_string(members.size()) + "):\n";
            for (const auto& m : members) {
                // We'd need to check roles properly, but for now just listing IDs
                list += "- " + m.GetId() + "\n";
            }
            msg.Reply(list);
        } catch (const std::exception& e) {
            msg.Reply("Error listing members: " + std::string(e.what()));
        }
    }
    else if (cmd == "/send-file") {
        std::string path; ss >> path;
        if (path.empty()) {
            msg.Reply("Usage: /send-file <path>");
            return;
        }
        try {
            if (msg.IsGroup()) msg.GetGroup().SendFile(path);
            else msg.GetAuthor().SendFile(path);
        } catch (const std::exception& e) {
            msg.Reply("Upload failed: " + std::string(e.what()));
        }
    }
    else if (cmd == "/react") {
        std::string emoji; ss >> emoji;
        if (emoji.empty()) emoji = "👍";
        msg.React(emoji);
    }
    else if (cmd == "/delete_member" || cmd == "/delete-member") {
        if (!msg.IsGroup()) {
            msg.Reply("This is not a group.");
            return;
        }
        std::string target; ss >> target;
        if (target.empty()) {
            msg.Reply("Usage: /delete_member <session_id>");
            return;
        }
        try {
            auto group = msg.GetGroup();
            if (!group.IsAdmin()) {
                msg.Reply("I am not an admin in this group.");
                return;
            }
            group.RemoveMember(target);
            // Also send "kick" message via internal low-level API if needed, 
            // but Group::RemoveMember handles the config update.
            // The high-level API currently doesn't expose SendDeleteMember directly
            // but we can access it via GetMessageService() if really needed.
            client.GetMessageService().SendDeleteMember(group.GetId(), {target});
            
            msg.Reply("Member removed: " + target);
        } catch (const std::exception& e) {
            msg.Reply("Action failed: " + std::string(e.what()));
        }
    }
}

int main(int argc, char** argv) {
    std::signal(SIGINT,  SigHandler);
    std::signal(SIGTERM, SigHandler);

    std::string seed = (argc > 1) ? argv[1] : "";
    
    // Instantiate high-level Client
    Session::Client client(seed);

    client.SetDisplayName("06 Group Bot Exemple");

    std::cout << "Group Bot running.\n";
    std::cout << "ID: " << client.GetMe().GetId() << "\n";
    if (seed.empty()) {
        std::cout << "Seed: " << client.GetMnemonic() << "\n";
    }
    std::cout << "Invite me to a group to start!\n";

    client.OnMessage([&](Session::Message msg) {
        // Ignore self
        if (msg.GetAuthor().GetId() == client.GetMe().GetId()) return;

        std::string context = msg.IsGroup() ? "[GROUP] " : "[DM] ";
        std::cout << "[MSG] " << context << msg.GetAuthor().GetId() << ": " << msg.GetContent() << "\n";

        if (!msg.GetContent().empty() && msg.GetContent()[0] == '/') {
            HandleCommand(client, msg);
        }
    });

    client.OnReaction([&](Session::User reactor, Session::Message target, std::string emoji, bool added) {
        std::string context = target.IsGroup() ? "[GROUP] " : "[DM] ";
        std::cout << "[REACT] " << context << reactor.GetId() 
                  << (added ? " added " : " removed ") << emoji 
                  << " on message " << target.GetId() << "\n";
        target.Reply("You reacted to the message with " + emoji);
    });

    client.Start();

    while (g_Running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Stopping...\n";
    client.Stop();
    return 0;
}
