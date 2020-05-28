#pragma once

#include <cstdlib>
#include <cstring>

#include <iostream>
#include <string>
#include <utility>
#include <stdexcept>
#include <filesystem>
#include <system_error>
#include <algorithm>
#include <string_view>

#include <boost/algorithm/hex.hpp>

#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cryptopp/cryptlib.h>
#include <cryptopp/sha3.h>
#include <cryptopp/rsa.h>
#include <cryptopp/osrng.h>
#include <cryptopp/files.h>
#include <cryptopp/queue.h>

#include "application.hpp"
#include "types.hpp"

namespace lmail
{

inline auto sha256(std::string const &data)
{
    using namespace CryptoPP;
    SHA3_256 sha256_algo;
    std::string        digest;
    digest.resize(sha256_algo.DigestSize());
    sha256_algo.CalculateDigest(reinterpret_cast<byte*>(digest.data()), reinterpret_cast<CryptoPP::byte const *>(data.data()), data.size());
    return boost::algorithm::hex_lower(digest);
}

inline auto uread(std::string &input, std::string_view prompt = {})
{
    std::cout << prompt;
    std::cin.clear();
    return static_cast<bool>(std::getline(std::cin, input));
}

inline auto uread_hidden(std::string &input, std::string_view prompt = {})
{
    termios ts;

    // disabling echoing on STDIN with preserving old flags
    tcgetattr(STDIN_FILENO, &ts);
    auto const old_flags = ts.c_lflag;
    ts.c_lflag &= ~ECHO;
    ts.c_lflag |= ECHONL;
    tcsetattr(STDIN_FILENO, TCSANOW, &ts);

    auto const res = uread(input, prompt);

    // restoring flags on STDIN
    tcgetattr(STDIN_FILENO, &ts);
    ts.c_lflag = old_flags;
    tcsetattr(STDIN_FILENO, TCSANOW, &ts);

    return res;
}

template <typename... Args>
void secure_memset(Args &&... args) noexcept
{
    auto volatile f = std::memset;
    f(std::forward<Args>(args)...);
}

namespace detail
{

template <typename KeyT>
void save_key(KeyT const &key, std::filesystem::path const &key_path, int perms)
{
    using namespace CryptoPP;
    auto const fd = creat(key_path.c_str(), perms);
    if (fd < 0)
        throw std::runtime_error("couldn't create a key-related file");
    close(fd);
    ByteQueue q;
    key.Save(q);
    FileSink file(key_path.c_str());
    q.CopyTo(file);
    file.MessageEnd();
}

} // namespace detail

template <typename UnaryFunction>
void for_each_dir_entry(std::filesystem::path const &dir, UnaryFunction f)
{
    std::error_code ec;
    for (auto const &dir_entry : std::filesystem::directory_iterator(dir, ec))
        f(dir_entry);
}

template <typename Comp, typename UnaryFunction>
void for_each_dir_entry_if(std::filesystem::path const &dir, Comp comp, UnaryFunction f)
{
    std::error_code ec;
    for (auto const &dir_entry : std::filesystem::directory_iterator(dir, ec))
    {
        if (comp(dir_entry))
            f(dir_entry);
    }
}

template <typename Comp>
auto find_dir_entry_if(std::filesystem::path const &dir, Comp comp)
{
    namespace fs = std::filesystem;
    fs::directory_entry dir_entry;
    std::error_code ec;
    auto const rng    = fs::directory_iterator(dir, ec);
    auto dir_entry_it = std::find_if(fs::begin(rng), fs::end(rng), comp);
    if (fs::end(rng) != dir_entry_it)
        dir_entry = *dir_entry_it;
    return dir_entry;
}

inline auto find_assoc(std::filesystem::path const &dir, std::string_view username)
{
    return find_dir_entry_if(dir, [username](auto const &dir_entry){
        return dir_entry.path().filename().stem() == username;
    }).path();
}

inline auto find_keyimp(std::filesystem::path const &dir, std::string_view username) { return find_assoc(dir, username); }

inline auto find_key_pair_dir(std::filesystem::path const &dir, std::string_view keyname)
{
    return find_dir_entry_if(dir, [keyname](auto const &dir_entry){ return dir_entry.path().filename() == keyname; }).path();
}

inline void create_rsa_key(std::filesystem::path const &keys_pair_dir, size_t keysize)
{
    using namespace CryptoPP;
    namespace fs = std::filesystem;

    auto save_priv_key = [](auto const &key, fs::path const &keypath) {
        detail::save_key(key, keypath, S_IRUSR | S_IWUSR);
    };

    auto save_pub_key = [](auto const &key, fs::path const &keypath) {
        detail::save_key(key, keypath, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    };

    if (fs::exists(keys_pair_dir) && !fs::is_directory(keys_pair_dir)
        || !fs::exists(keys_pair_dir) && !fs::create_directories(keys_pair_dir))
    {
        throw std::runtime_error("couldn't create key pair");
    }

    AutoSeededRandomPool rnd;
    RSA::PrivateKey priv_key;
    priv_key.GenerateRandomWithKeySize(rnd, keysize);
    auto key_path = keys_pair_dir / Application::kPrivKeyName;
    save_priv_key(priv_key, key_path);
    key_path += Application::kPubKeySuffix;
    save_pub_key(RSA::PublicKey(priv_key), key_path);
}

} // namespace lmail
