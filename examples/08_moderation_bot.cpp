/**
 * Example 08 – Moderation Bot
 * ─────────────────────────────────────────────────────────
 * A bot that moderates groups:
 *   - DM: tells user to add it to a group and give admin
 *   - Group: /help, regex-based auto-delete of banned messages
 *   - Auto-accepts admin promotion
 *   - Verbose mode (-v) for JSON debug logging
 *
 * Usage:
 *   ./08_moderation_bot [-v] [seed]
 *   If no seed provided, a new account is created.
 */

#include <SessionAppFramework/Session.hpp>
#include <SessionAppFramework/Utils.hpp>
#include <SessionAppFramework/Types.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <regex>
#include <map>
#include <vector>
#include <mutex>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static std::atomic<bool> g_Running{true};
static std::atomic<bool> g_Verbose{false};
void SigHandler(int) { g_Running = false; }

struct GroupRule {
    std::regex Pattern;
    std::string Reason;
    std::string PatternStr;
};

void LogJson(const std::string& label, const json& j) {
    if (g_Verbose) {
        std::cout << "[JSON " << label << "] " << j.dump() << "\n";
    }
}

std::string MsgToJson(Session::Message& msg) {
    json j;
    j["id"] = msg.GetId();
    j["content"] = msg.GetContent();
    j["isGroup"] = msg.IsGroup();
    j["hasFile"] = msg.HasFile();
    if (msg.IsGroup()) {
        try {
            j["groupId"] = msg.GetGroup().GetId();
            j["groupName"] = msg.GetGroup().GetName();
        } catch (...) {}
    }
    j["author"] = msg.GetAuthor().GetId();
    return j.dump();
}

void LogMsg(const std::string& prefix, Session::Message& msg) {
    auto content = msg.GetContent();
    if (content.size() > 80) content = content.substr(0, 80) + "...";
    auto author = msg.GetAuthor().GetId().substr(0, 8);
    if (msg.IsGroup()) {
        std::cout << prefix << " [GROUP " << author << "] " << content << "\n";
    } else {
        std::cout << prefix << " [DM " << author << "] " << content << "\n";
    }
}

class ModBot {
public:
    ModBot(const std::string& seed = "")
        : m_Client(seed) {}

    void Start() {
        m_Client.SetDisplayName("ModBot");

        m_Client.OnMessage([this](Session::Message msg) {
            try {
                LogMsg("[RCV]", msg);
                if (g_Verbose) {
                    std::cout << "[VERBOSE] " << MsgToJson(msg) << "\n";
                }
                HandleMessage(msg);
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] " << e.what() << "\n";
            }
        });

        m_Client.OnGroupPromotedToAdmin([this](Session::Group group) {
            std::cout << "[Bot] ===== PROMOTED TO ADMIN =====\n";
            std::cout << "[Bot] Group ID:  " << group.GetId() << "\n";
            std::cout << "[Bot] Name:      " << group.GetName() << "\n";
            std::cout << "[Bot] Admin:     " << (group.IsAdmin() ? "YES" : "NO") << "\n";
            std::cout << "[Bot] Members:   " << group.GetMembers().size() << "\n";
            group.SendMessage("ModBot is now admin. Use /help for commands.");
            if (g_Verbose) {
                json j;
                j["event"] = "promoted";
                j["groupId"] = group.GetId();
                j["groupName"] = group.GetName();
                j["admin"] = group.IsAdmin();
                LogJson("ModAction", j);
            }
        });

        m_Client.Start();
        LoadState();
        std::cout << "ModBot started: " << m_Client.GetMnemonic() << "\n";
    }

    void Stop() {
        SaveState();
        m_Client.Stop();
    }

    std::string GetId() {
        return m_Client.GetMe().GetId();
    }

    void PrintStatus() {
        auto groups = m_Client.GetGroupManager().GetAll();
        std::cout << "[HB] Alive, " << groups.size() << " group(s):";
        for (const auto& g : groups)
            std::cout << " " << g.Name << "(" << (g.IsAdmin ? "admin" : "member") << ")";
        std::cout << "\n";
    }

    void SaveState() {
        auto id = GetId().substr(0, 16);
        std::string path = "modbot_" + id + ".json";
        json j;
        for (const auto& [gid, rules] : m_Rules) {
            json rlist = json::array();
            for (const auto& rule : rules) {
                rlist.push_back({{"pattern", rule.PatternStr}, {"reason", rule.Reason}});
            }
            j["groups"][gid] = rlist;
        }
        std::ofstream f(path);
        if (f) f << j.dump(2);
    }

    void LoadState() {
        auto id = GetId().substr(0, 16);
        std::string path = "modbot_" + id + ".json";
        std::ifstream f(path);
        if (!f) return;
        try {
            json j = json::parse(f);
            if (j.contains("groups")) {
                for (auto& [gid, rlist] : j["groups"].items()) {
                    for (auto& r : rlist) {
                        try {
                            m_Rules[gid].push_back({
                                std::regex(r["pattern"].get<std::string>()),
                                r["reason"].get<std::string>(),
                                r["pattern"].get<std::string>()
                            });
                        } catch (...) {}
                    }
                }
            }
            std::cout << "[Bot] Loaded " << (j.contains("groups") ? j["groups"].size() : 0)
                      << " group rules from " << path << "\n";
        } catch (...) {}
    }

private:
    void HandleMessage(Session::Message& msg) {
        if (msg.IsGroup()) {
            HandleGroupMessage(msg);
        } else {
            HandleDirectMessage(msg);
        }
    }

    void HandleDirectMessage(Session::Message& msg) {
        msg.Reply("add me to a group, and give me admin to start");
    }

    void HandleGroupMessage(Session::Message& msg) {
        auto group = msg.GetGroup();
        auto content = msg.GetContent();
        auto author = msg.GetAuthor();

        // Commands
        if (content.rfind("/", 0) == 0) {
            HandleCommand(msg, group);
            return;
        }

        // Auto-delete only when admin
        if (!group.IsAdmin()) {
            auto info = m_Client.GetGroupManager().Get(group.GetId());
            if (!info.IsAdmin) {
                // Not admin - ignore non-command messages
                return;
            }
            // Admin flag just synced
        }

        auto it = m_Rules.find(group.GetId());
        if (it == m_Rules.end()) return;

        for (const auto& rule : it->second) {
            if (std::regex_search(content, rule.Pattern)) {
                if (g_Verbose) {
                    json j;
                    j["action"] = "delete";
                    j["message"] = content;
                    j["author"] = author.GetId();
                    j["reason"] = rule.Reason;
                    j["group"] = group.GetId();
                    LogJson("ModAction", j);
                }
                msg.Delete();
                group.SendMessage(
                    "[ModBot] Deleted message from " + author.GetId().substr(0, 8)
                    + " (reason: " + rule.Reason + ")");
                return;
            }
        }
    }

    void HandleCommand(Session::Message& msg, Session::Group& group) {
        auto content = msg.GetContent();

        std::string cmd, args;
        auto space = content.find(' ');
        if (space != std::string::npos) {
            cmd = content.substr(0, space);
            args = content.substr(space + 1);
        } else {
            cmd = content;
        }

        if (cmd == "/help") {
            ShowHelp(msg, group);
        } else if (cmd == "/rules") {
            ListRules(msg, group);
        } else if (cmd == "/addrule") {
            AddRule(msg, group, args);
        } else if (cmd == "/rmrule") {
            RemoveRule(msg, group, args);
        } else {
            msg.Reply("Unknown command. Use /help");
        }
    }

    void ShowHelp(Session::Message& msg, Session::Group& group) {
        if (!group.IsAdmin()) {
            // Re-check group manager directly in case config hasn't synced yet
            auto info = m_Client.GetGroupManager().Get(group.GetId());
            if (info.IsAdmin) {
                std::cout << "[Bot] Admin flag synced from GroupManager\n";
            } else {
                msg.Reply("ModBot is not admin. Give me admin to moderate.");
                return;
            }
        }
        msg.Reply(
            "ModBot Commands:\n"
            "/help           - Show this help\n"
            "/rules          - List auto-delete rules\n"
            "/addrule <re> <reason> - Add a regex rule\n"
            "/rmrule <N>     - Remove rule N");
    }

    void ListRules(Session::Message& msg, Session::Group& group) {
        if (!group.IsAdmin()) {
            msg.Reply("ModBot is not admin.");
            return;
        }
        auto it = m_Rules.find(group.GetId());
        if (it == m_Rules.end() || it->second.empty()) {
            msg.Reply("No rules defined.");
            return;
        }
        std::string out = "Rules:\n";
        for (size_t i = 0; i < it->second.size(); i++) {
            out += std::to_string(i) + ": " + it->second[i].Reason + "\n";
        }
        msg.Reply(out);
    }

    void AddRule(Session::Message& msg, Session::Group& group, const std::string& args) {
        if (!group.IsAdmin()) {
            msg.Reply("ModBot is not admin.");
            return;
        }
        auto space = args.find(' ');
        if (space == std::string::npos || space == 0 || space + 1 >= args.size()) {
            msg.Reply("Usage: /addrule <regex> <reason>");
            return;
        }
        std::string pattern = args.substr(0, space);
        std::string reason = args.substr(space + 1);
        try {
            std::regex re(pattern);
            m_Rules[group.GetId()].push_back({std::move(re), reason, pattern});
            msg.Reply("Rule added: " + reason);
            if (g_Verbose) {
                json j;
                j["action"] = "addrule";
                j["pattern"] = pattern;
                j["reason"] = reason;
                j["group"] = group.GetId();
                LogJson("ModAction", j);
            }
        } catch (const std::regex_error& e) {
            msg.Reply("Invalid regex: " + std::string(e.what()));
        }
    }

    void RemoveRule(Session::Message& msg, Session::Group& group, const std::string& args) {
        if (!group.IsAdmin()) {
            msg.Reply("ModBot is not admin.");
            return;
        }
        auto& rules = m_Rules[group.GetId()];
        try {
            size_t idx = std::stoul(args);
            if (idx >= rules.size()) {
                msg.Reply("Invalid rule index.");
                return;
            }
            rules.erase(rules.begin() + static_cast<long>(idx));
            msg.Reply("Rule removed.");
        } catch (...) {
            msg.Reply("Usage: /rmrule <N>");
        }
    }

    Session::Client m_Client;
    std::map<std::string, std::vector<GroupRule>> m_Rules;
};

int main(int argc, char** argv) {
    signal(SIGINT, SigHandler);
    signal(SIGTERM, SigHandler);

    std::string seed;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose") {
            g_Verbose = true;
        } else {
            seed = arg;
        }
    }

    try {
        ModBot bot(seed);
        bot.Start();
        std::cout << "\nBot ID: " << bot.GetId() << "\n";
        std::cout << "Verbose: " << (g_Verbose ? "ON" : "OFF") << "\n";
        std::cout << "Add this bot to a group and promote it to admin.\n";
        std::cout << "Example: /addrule scam|spam Spam detection\n";
        std::cout << "Press Ctrl+C to stop.\n\n";

        int heartbeat = 0;
        while (g_Running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (++heartbeat % 30 == 0) {
                bot.PrintStatus();
            }
        }

        bot.Stop();
        std::cout << "ModBot stopped.\n";
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
