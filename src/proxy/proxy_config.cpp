#include "proxy_config.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cctype>

namespace notiman {

namespace {

std::string narrow_utf8(const wchar_t* value) {
    if (value == nullptr || value[0] == L'\0') {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) {
        return {};
    }

    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), size, nullptr, nullptr);
    result.pop_back();
    return result;
}

std::string trim(const std::string& input) {
    size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
        ++start;
    }

    size_t end = input.size();
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
        --end;
    }

    return input.substr(start, end - start);
}

std::vector<ProxyRoute> load_routes(const std::wstring& ini_path) {
    std::vector<ProxyRoute> routes;

    std::vector<wchar_t> section_data(32768, L'\0');
    const DWORD copied = GetPrivateProfileSectionW(
        L"routes",
        section_data.data(),
        static_cast<DWORD>(section_data.size()),
        ini_path.c_str());

    if (copied == 0) {
        return routes;
    }

    const wchar_t* cursor = section_data.data();
    while (*cursor != L'\0') {
        std::wstring line(cursor);
        const size_t equals_pos = line.find(L'=');
        if (equals_pos != std::wstring::npos) {
            const std::wstring key_w = line.substr(0, equals_pos);
            const std::wstring value_w = line.substr(equals_pos + 1);
            const std::string key = trim(narrow_utf8(key_w.c_str()));
            const std::string value = trim(narrow_utf8(value_w.c_str()));

            if (!key.empty() && !value.empty()) {
                routes.push_back(ProxyRoute{key, value});
            }
        }
        cursor += line.size() + 1;
    }

    std::sort(routes.begin(), routes.end(), [](const ProxyRoute& a, const ProxyRoute& b) {
        return a.path_prefix.size() > b.path_prefix.size();
    });

    return routes;
}

}  // namespace

ProxyConfig ProxyConfig::load_from_file(const std::filesystem::path& path) {
    ProxyConfig config;
    if (!std::filesystem::exists(path)) {
        return config;
    }

    const std::wstring ini_path = path.wstring();

    wchar_t host_buf[256] = {};
    GetPrivateProfileStringW(
        L"proxy", L"host", L"127.0.0.1", host_buf, static_cast<DWORD>(_countof(host_buf)), ini_path.c_str());
    config.host = narrow_utf8(host_buf);
    if (config.host.empty()) {
        config.host = "127.0.0.1";
    }

    config.port = static_cast<int>(GetPrivateProfileIntW(L"proxy", L"port", config.port, ini_path.c_str()));
    if (config.port <= 0 || config.port > 65535) {
        config.port = 8080;
    }

    config.routes = load_routes(ini_path);
    return config;
}

std::filesystem::path ProxyConfig::default_config_path() {
    WCHAR appdata_path[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata_path) == S_OK) {
        std::filesystem::path primary(appdata_path);
        primary /= "notiman";
        primary /= "proxy.ini";
        if (std::filesystem::exists(primary)) {
            return primary;
        }
    }

    WCHAR exe_path[MAX_PATH];
    if (GetModuleFileNameW(NULL, exe_path, MAX_PATH) > 0) {
        std::filesystem::path fallback(exe_path);
        fallback = fallback.parent_path() / "config" / "proxy.ini";
        return fallback;
    }

    return "config/proxy.ini";
}

}  // namespace notiman
