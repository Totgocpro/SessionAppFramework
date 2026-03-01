/**
 * Example 02 – Send and Receive DM
 * ─────────────────────────────────────────────────────────
 */

#include <SessionAppFramework/Session.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>

static std::atomic<bool> g_Running{true};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <seed_or_mnemonic> <recipient_id>\n";
        return 1;
    }

    Session::Client client(argv[1]);
    std::string recipientId = argv[2];

    client.OnMessage([&](Session::Message msg) {
        std::cout << "[MSG] From " << msg.GetAuthor().GetId() << ": " << msg.GetContent() << "\n";
    });

    client.Start();

    std::cout << "Sending 'Hello Session!' to " << recipientId << "...\n";
    try {
        client.GetUser(recipientId).SendMessage("Hello Session!");
        std::cout << "Sent.\n";
    } catch (const std::exception& e) {
        std::cerr << "Failed to send: " << e.what() << "\n";
    }

    std::cout << "Listening for 10 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(10));

    client.Stop();
    return 0;
}
