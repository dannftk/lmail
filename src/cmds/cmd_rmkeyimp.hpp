#pragma once

#include <cstdlib>

#include <system_error>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <algorithm>

#include <boost/range/algorithm_ext/erase.hpp>

#include "types.hpp"
#include "utility.hpp"
#include "application.hpp"

namespace lmail
{

class CmdRmKeyImp
{
public:
    explicit CmdRmKeyImp(args_t args, std::filesystem::path profile_path)
        : args_(std::move(args))
        , profile_path_(std::move(profile_path))
    {
        if (profile_path_.empty())
            throw std::invalid_argument("profile path provided cannot be empty");
        boost::remove_erase_if(args_, [](auto const &arg){ return arg.empty(); });
    }

    void operator()()
    try
    {
        namespace fs = std::filesystem;

        username_t username_tgt;
        if (!args_.empty())
        {
            username_tgt = args_.front();
        }
        else if (!uread(username_tgt, "Enter a target user name: "))
        {
            return;
        }
        else if (username_tgt.empty())
        {
            std::cerr << "target user name is not specified\n";
            return;
        }

        if (auto const key_path = find_keyimp(profile_path_ / Application::kCypherDirName, username_tgt); !key_path.empty())
        {
            auto extract_keyname = [](auto const &key_path){ return key_path.filename().string(); };
            auto const keyname = extract_keyname(key_path);
            if (keyname.empty())
            {
                std::cerr << "couldn't extract key name\n";
                return;
            }

            std::cout << "The imported key '" << keyname << "' for user '" << username_tgt << "' is about to be removed" << std::endl;
            std::string ans;
            while (uread(ans, "Remove it? (y/n): ") && ans != "y" && ans != "n")
                ;
            if ("y" == ans)
            {
                std::error_code ec;
                fs::remove(key_path, ec);
                if (!ec)
                    std::cout << "imported key '" << keyname << "' for user '" << username_tgt << "' successfully removed\n";
                else
                    std::cerr << "failed to remove the imported key '" << keyname << "' for user '" << username_tgt << "'\n";
            }
        }
        else
        {
            std::cerr << "There is none public key associated with user '" << username_tgt << "'\n";
        }
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
    args_t                args_;
    std::shared_ptr<User> user_;
    std::filesystem::path profile_path_;
};

} // namespace lmail
