/**
 * Example 03 – Group Echo Bot
 * ─────────────────────────────────────────────────────────
 * Joins groups and echoes messages.
 */

#include <SessionAppFramework/Session.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>

static std::atomic<bool> g_Running{true};
void SigHandler(int) { g_Running = false; }

int main(int argc, char** argv) {
    std::signal(SIGINT, SigHandler);
    std::string seed = (argc > 1) ? argv[1] : "";
    Session::Client client(seed);

    std::cout << "Echo Bot ID: " << client.GetMe().GetId() << "\n";

    client.OnMessage([&](Session::Message msg) {
        if (msg.GetAuthor().GetId() == client.GetMe().GetId()) return; // Ignore self

        std::cout << "Received: " << msg.GetContent() << "\n";
        msg.Reply("Echo: " + msg.GetContent());
    });

    client.Start();

    while (g_Running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    client.Stop();
    return 0;
}
