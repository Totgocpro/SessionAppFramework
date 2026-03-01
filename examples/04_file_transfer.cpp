/**
 * Example 04 – File Transfer
 * ─────────────────────────────────────────────────────────
 */

#include <SessionAppFramework/Session.hpp>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <seed> <recipient> <file_path>\n";
        return 1;
    }

    Session::Client client(argv[1]);
    std::string recipient = argv[2];
    std::string file = argv[3];

    client.Start();

    try {
        std::cout << "Sending file to " << recipient << "...\n";
        client.GetUser(recipient).SendFile(file);
        std::cout << "Done.\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    client.Stop();
    return 0;
}
