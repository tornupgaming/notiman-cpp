#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace notiman {

struct ProxyRoute {
    std::string path_prefix;
    std::string target_base_url;
};

struct ProxyConfig {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::vector<ProxyRoute> routes;

    static ProxyConfig load_from_file(const std::filesystem::path& path);
    static std::filesystem::path default_config_path();
};

}  // namespace notiman
