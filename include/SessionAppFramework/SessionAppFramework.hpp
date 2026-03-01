#pragma once

/**
 * @file SessionAppFramework.hpp
 * @brief Single-include header for the SessionAppFramework library.
 *
 * Include this file to access all framework components:
 *
 * @code
 * #include <SessionAppFramework/SessionAppFramework.hpp>
 *
 * int main() {
 *     Saf::Account account;
 *     account.LoadFromMnemonic("word1 word2 ... word25");
 *
 *     Saf::NetworkClient net;
 *     Saf::SwarmManager  swarm(account, net);
 *     swarm.Bootstrap();
 *
 *     Saf::ConfigManager config(account, swarm);
 *     config.Pull();
 *     config.SetDisplayName("MyBot");
 *     config.Push();
 *
 *     Saf::MessageService ms(account, swarm);
 *     ms.OnMessage([](const Saf::Message& m) {
 *         std::cout << m.Sender << ": " << m.Body << "\n";
 *     });
 *     ms.StartPolling();
 *
 *     ms.SendText("05recipient_account_id", "Hello from SAF!");
 * }
 * @endcode
 */

#include "Types.hpp"
#include "Exceptions.hpp"
#include "Utils.hpp"
#include "Account.hpp"
#include "NetworkClient.hpp"
#include "SessionNode.hpp"
#include "SwarmManager.hpp"
#include "ConfigManager.hpp"
#include "MessageService.hpp"
#include "GroupManager.hpp"
#include "FileTransfer.hpp"
#include "OnionRouter.hpp"
