#pragma once

#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include "application.hpp"
#include "cmd_args.hpp"
#include "logged_user.hpp"
#include "types.hpp"
#include "uread.hpp"
#include "utility.hpp"

namespace lmail
{

class CmdKeyAssoc final
{
public:
    explicit CmdKeyAssoc(CmdArgs args, std::shared_ptr<LoggedUser> logged_user)
        : args_(std::move(args)), logged_user_(std::move(logged_user))
    {
        if (!logged_user_)
            throw std::invalid_argument("logged user provided cannot be empty");
    }

    void operator()()
    try
    {
        namespace fs = std::filesystem;

        auto args = args_;

        keyname_t keyname = args.pop();
        if (keyname.empty() && !uread(keyname, "Enter key name: "))
            return;

        if (keyname.empty())
        {
            std::cerr << "key name is not specified\n";
            return;
        }

        auto const &keys_pair_dir = logged_user_->profile().find_key(keyname);
        if (keys_pair_dir.empty())
        {
            std::cerr << "There is no key '" << keyname << "'\n";
            return;
        }

        username_t username_tgt = args.pop();
        if (username_tgt.empty() && !uread(username_tgt, "Enter a target user name the key is linked to: "))
            return;

        if (username_tgt.empty())
        {
            std::cerr << "target user name cannot be empty\n";
            return;
        }

        username_tgt = fs::path(username_tgt).filename();
        if (username_tgt.empty())
        {
            std::cerr << "target user name cannot be empty\n";
            return;
        }

        if (logged_user_->name() == username_tgt)
        {
            std::cerr << "you cannot create a new association between your key and yourself\n";
            return;
        }

        std::cout << "trying to link key '" << keyname << "' to user '" << username_tgt << "' ...\n";

        auto const assocs_dir = logged_user_->profile().assocs_dir();
        if (!create_dir_if_doesnt_exist(assocs_dir))
            throw std::runtime_error("failed to create key association");

        auto assoc_path = assocs_dir / username_to_keyname(username_tgt);
        assoc_path += Application::kUserKeyLinkSuffix;
        if (fs::exists(assoc_path))
        {
            std::cerr << "some key is already associated with user '" << username_tgt << "'\n";
            return;
        }

        std::error_code ec;
        fs::create_directory_symlink(keys_pair_dir, assoc_path, ec);
        if (!ec)
            std::cout << "successfully created key-user association" << std::endl;
        else
            std::cerr << "failed to create association between key '" << keyname << "' and the user '"
                      << username_tgt << "', reason: " << ec.message() << '\n';
    }
    catch (std::exception const &ex)
    {
        std::cerr << "error occurred: " << ex.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "unknown exception\n";
    }

private:
    CmdArgs                     args_;
    std::shared_ptr<LoggedUser> logged_user_;
};

} // namespace lmail
