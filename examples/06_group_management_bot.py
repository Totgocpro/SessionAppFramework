import session_saf
import time
import sys
import signal
import os

def handle_command(client, msg):
    body = msg.get_content()
    parts = body.split()
    if not parts:
        return
    
    cmd = parts[0].lower()
    sender_id = msg.get_author().get_id()

    if cmd == "/help":
        help_text = (
            "Python Commands:\n"
            "/id - Show conversation ID\n"
            "/members - List group members\n"
            "/send-file <path> - Send a file\n"
            "/react <emoji> - React to this command\n"
            "/delete_member <id> - Kick a member (Admin only)"
        )
        msg.reply(help_text)
    
    elif cmd == "/id":
        dest = msg.get_group().get_id() if msg.is_group() else sender_id
        msg.reply(f"Conversation ID: {dest}")

    elif cmd == "/members":
        if not msg.is_group():
            msg.reply("This is not a group.")
            return
        try:
            group = msg.get_group()
            members = group.get_members()
            reply = f"Members ({len(members)}):\n" + "\n".join([f"- {m.get_id()}" for m in members])
            msg.reply(reply)
        except Exception as e:
            msg.reply(f"Error: {str(e)}")

    elif cmd == "/send-file":
        if len(parts) < 2:
            msg.reply("Usage: /send-file <path>")
            return
        path = parts[1]
        try:
            if msg.is_group():
                msg.get_group().send_file(path)
            else:
                msg.get_author().send_file(path)
        except Exception as e:
            msg.reply(f"Upload failed: {str(e)}")

    elif cmd == "/react":
        emoji = parts[1] if len(parts) > 1 else "👍"
        msg.react(emoji)

    elif cmd in ["/delete_member", "/delete-member"]:
        if not msg.is_group():
            msg.reply("This is not a group.")
            return
        if len(parts) < 2:
            msg.reply("Usage: /delete_member <session_id>")
            return
        target = parts[1]
        try:
            group = msg.get_group()
            if not group.is_admin():
                msg.reply("I am not an admin here.")
                return
            group.remove_member(target)
            msg.reply(f"Member removed: {target}")
        except Exception as e:
            msg.reply(f"Action failed: {str(e)}")

def main():
    seed = sys.argv[1] if len(sys.argv) > 1 else ""
    client = session_saf.Client(seed)

    print(f"--- Python Group Bot ---")
    print(f"ID:   {client.get_me().get_id()}")
    if not seed:
        print(f"Seed: {client.get_mnemonic()}")
    
    client.set_display_name("Python Mod Bot")
    client.set_message_db_path("python_bot.db")

    def on_message(msg):
        if msg.get_author().get_id() == client.get_me().get_id():
            return
        
        ctx = "[GROUP]" if msg.is_group() else "[DM]"
        print(f"{ctx} {msg.get_author().get_id()}: {msg.get_content()}")
        
        if msg.get_content().startswith("/"):
            handle_command(client, msg)

    def on_reaction(reactor, target, emoji, added):
        action = "added" if added else "removed"
        print(f"[REACT] {reactor.get_id()} {action} {emoji} on {target.get_id()}")

    client.on_message(on_message)
    client.on_reaction(on_reaction)

    client.start()
    print("Bot online. Ctrl+C to quit.")

    def signal_handler(sig, frame):
        print("\nShutting down...")
        client.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    
    while True:
        time.sleep(1)

if __name__ == "__main__":
    main()
