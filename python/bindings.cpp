#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <SessionAppFramework/Session.hpp>

namespace py = pybind11;

// Helper to handle Python GIL in callbacks
void wrap_message_callback(const py::function& callback, Session::Message msg) {
    py::gil_scoped_acquire acquire;
    callback(msg);
}

void wrap_reaction_callback(const py::function& callback, Session::User reactor, Session::Message target, std::string emoji, bool added) {
    py::gil_scoped_acquire acquire;
    callback(reactor, target, emoji, added);
}

PYBIND11_MODULE(session_saf, m) {
    m.doc() = "Python bindings for SessionAppFramework (SAF)";

    // ─────────────────────────────────────────────────────────
    // User
    // ─────────────────────────────────────────────────────────
    py::class_<Session::User>(m, "User")
        .def("get_id", &Session::User::GetId)
        .def("get_display_name", &Session::User::GetDisplayName)
        .def("send_message", &Session::User::SendMessage, py::arg("text"))
        .def("send_file", &Session::User::SendFile, py::arg("file_path"));

    // ─────────────────────────────────────────────────────────
    // Group
    // ─────────────────────────────────────────────────────────
    py::class_<Session::Group>(m, "Group")
        .def("get_id", &Session::Group::GetId)
        .def("get_name", &Session::Group::GetName)
        .def("get_description", &Session::Group::GetDescription)
        .def("get_members", &Session::Group::GetMembers)
        .def("is_admin", &Session::Group::IsAdmin)
        .def("send_message", &Session::Group::SendMessage, py::arg("text"))
        .def("send_file", &Session::Group::SendFile, py::arg("file_path"))
        .def("leave", &Session::Group::Leave)
        .def("set_name", &Session::Group::SetName, py::arg("name"))
        .def("add_member", &Session::Group::AddMember, py::arg("user_id"))
        .def("remove_member", &Session::Group::RemoveMember, py::arg("user_id"))
        .def("promote_member", &Session::Group::PromoteMember, py::arg("user_id"))
        .def("demote_member", &Session::Group::DemoteMember, py::arg("user_id"));

    // ─────────────────────────────────────────────────────────
    // Message
    // ─────────────────────────────────────────────────────────
    py::class_<Session::Message>(m, "Message")
        .def("get_id", &Session::Message::GetId)
        .def("get_content", &Session::Message::GetContent)
        .def("get_author", &Session::Message::GetAuthor)
        .def("is_group", &Session::Message::IsGroup)
        .def("get_group", &Session::Message::GetGroup)
        .def("has_file", &Session::Message::HasFile)
        .def("get_file_name", &Session::Message::GetFileName)
        .def("get_file_size", &Session::Message::GetFileSize)
        .def("save_file", &Session::Message::SaveFile, py::arg("dest_path"))
        .def("reply", &Session::Message::Reply, py::arg("text"))
        .def("react", &Session::Message::React, py::arg("emoji"))
        .def("mark_as_read", &Session::Message::MarkAsRead);

    // ─────────────────────────────────────────────────────────
    // Client
    // ─────────────────────────────────────────────────────────
    py::class_<Session::Client>(m, "Client")
        .def(py::init<const std::string&>(), py::arg("seed") = "")
        .def("start", [](Session::Client &self) {
            py::gil_scoped_release release;
            self.Start();
        })
        .def("stop", [](Session::Client &self) {
            py::gil_scoped_release release;
            self.Stop();
        })
        .def("get_me", &Session::Client::GetMe)
        .def("get_mnemonic", &Session::Client::GetMnemonic)
        .def("set_display_name", &Session::Client::SetDisplayName, py::arg("name"))
        .def("set_profile_picture", &Session::Client::SetProfilePicture, py::arg("file_path"))
        .def("set_message_db_path", &Session::Client::SetMessageDbPath, py::arg("path"))
        .def("get_message_db_path", &Session::Client::GetMessageDbPath)
        .def("on_message", [](Session::Client &self, py::function cb) {
            self.OnMessage([cb](Session::Message msg) {
                wrap_message_callback(cb, msg);
            });
        })
        .def("on_reaction", [](Session::Client &self, py::function cb) {
            self.OnReaction([cb](Session::User reactor, Session::Message target, std::string emoji, bool added) {
                wrap_reaction_callback(cb, reactor, target, emoji, added);
            });
        })
        .def("create_group", &Session::Client::CreateGroup, py::arg("name"))
        .def("get_group", &Session::Client::GetGroup, py::arg("group_id"))
        .def("get_user", &Session::Client::GetUser, py::arg("user_id"));
}
