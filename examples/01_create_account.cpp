/**
 * Example 01 – Create or Load Account
 * ─────────────────────────────────────────────────────────
 * Demonstrates basic Session::Client initialization.
 */

#include <SessionAppFramework/Session.hpp>
#include <iostream>

int main(int argc, char** argv) {
    if (argc > 1) {
        // Load existing
        std::string seed = argv[1];
        Session::Client client(seed);
        std::cout << "Loaded Account: " << client.GetMe().GetId() << "\n";
    } else {
        // Create new
        Session::Client client;
        std::cout << "Created New Account\n";
        std::cout << "ID:   " << client.GetMe().GetId() << "\n";
        std::cout << "Seed: " << client.GetMnemonic() << "\n";
    }
    return 0;
}
