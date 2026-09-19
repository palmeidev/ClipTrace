#include "updates.h"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <winver.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cwctype>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "version.lib")

namespace ClipTraceUpdates {
namespace {
constexpr wchar_t ReleasesApi[] = L"https://api.github.com/repos/palmeidev/ClipTrace/releases?per_page=20";
constexpr wchar_t AssetPrefix[] = L"https://github.com/palmeidev/ClipTrace/releases/download/";
constexpr uint64_t MaximumAssetSize = 100ULL * 1024ULL * 1024ULL;
std::atomic_bool checking = false;
std::atomic_bool installing = false;
std::atomic<ULONGLONG> lastCheck = 0;

struct InternetHandle {
    HINTERNET value = nullptr;
    ~InternetHandle() { if (value) WinHttpCloseHandle(value); }
};

struct Version {
    std::array<unsigned, 3> parts{};
    std::wstring suffix;
};

std::wstring ProductVersion(const std::wstring& path) {
    DWORD unused = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &unused);
    if (!size) return L"";
    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data())) return L"";
    struct Translation { WORD language; WORD codepage; };
    Translation* translations = nullptr;
    UINT length = 0;
    if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&translations), &length) || length < sizeof(Translation)) return L"";
    wchar_t key[80]{};
    swprintf_s(key, L"\\StringFileInfo\\%04x%04x\\ProductVersion", translations[0].language, translations[0].codepage);
    wchar_t* version = nullptr;
    if (!VerQueryValueW(data.data(), key, reinterpret_cast<void**>(&version), &length) || !version || !length) return L"";
    return version;
}

bool MatchesReleaseVersion(const std::wstring& path, const std::wstring& tag) {
    const std::wstring expected = !tag.empty() && (tag[0] == L'v' || tag[0] == L'V') ? tag.substr(1) : tag;
    const std::wstring actual = ProductVersion(path);
    return !actual.empty() && _wcsicmp(actual.c_str(), expected.c_str()) == 0;
}

bool ParseVersion(std::wstring value, Version& result) {
    if (value.empty() || value.size() > 64) return false;
    if (!value.empty() && (value[0] == L'v' || value[0] == L'V')) value.erase(0, 1);
    const size_t dash = value.find(L'-');
    if (dash != std::wstring::npos) {
        result.suffix = value.substr(dash + 1);
        value.resize(dash);
        if (result.suffix.empty()) return false;
        for (auto& character : result.suffix) {
            character = towlower(character);
            if (!((character >= L'a' && character <= L'z') || (character >= L'0' && character <= L'9') || character == L'.' || character == L'-')) return false;
        }
    }
    size_t start = 0;
    for (size_t index = 0; index < result.parts.size(); ++index) {
        const size_t dot = value.find(L'.', start);
        const std::wstring number = value.substr(start, dot == std::wstring::npos ? dot : dot - start);
        if (number.empty()) return false;
        unsigned part = 0;
        for (wchar_t character : number) {
            if (character < L'0' || character > L'9' || part > 1000000) return false;
            part = part * 10 + character - L'0';
        }
        result.parts[index] = part;
        if (dot == std::wstring::npos) return true;
        start = dot + 1;
    }
    return start >= value.size();
}

int CompareVersions(const Version& left, const Version& right) {
    if (left.parts != right.parts) return left.parts > right.parts ? 1 : -1;
    if (left.suffix.empty() != right.suffix.empty()) return left.suffix.empty() ? 1 : -1;
    size_t leftStart = 0, rightStart = 0;
    while (leftStart < left.suffix.size() && rightStart < right.suffix.size()) {
        const size_t leftEnd = left.suffix.find(L'.', leftStart);
        const size_t rightEnd = right.suffix.find(L'.', rightStart);
        const std::wstring leftPart = left.suffix.substr(leftStart, leftEnd == std::wstring::npos ? leftEnd : leftEnd - leftStart);
        const std::wstring rightPart = right.suffix.substr(rightStart, rightEnd == std::wstring::npos ? rightEnd : rightEnd - rightStart);
        const bool leftNumber = !leftPart.empty() && std::all_of(leftPart.begin(), leftPart.end(), iswdigit);
        const bool rightNumber = !rightPart.empty() && std::all_of(rightPart.begin(), rightPart.end(), iswdigit);
        if (leftNumber && rightNumber) {
            const unsigned long leftValue = wcstoul(leftPart.c_str(), nullptr, 10);
            const unsigned long rightValue = wcstoul(rightPart.c_str(), nullptr, 10);
            if (leftValue != rightValue) return leftValue > rightValue ? 1 : -1;
        } else if (leftNumber != rightNumber) {
            return leftNumber ? -1 : 1;
        } else if (leftPart != rightPart) {
            return leftPart > rightPart ? 1 : -1;
        }
        if (leftEnd == std::wstring::npos || rightEnd == std::wstring::npos) break;
        leftStart = leftEnd + 1;
        rightStart = rightEnd + 1;
    }
    if (leftStart < left.suffix.size() && rightStart < right.suffix.size()) {
        const bool leftMore = left.suffix.find(L'.', leftStart) != std::wstring::npos;
        const bool rightMore = right.suffix.find(L'.', rightStart) != std::wstring::npos;
        if (leftMore != rightMore) return leftMore ? 1 : -1;
    }
    return 0;
}

bool IsNewer(const std::wstring& candidate) {
    Version installed, available;
    wchar_t self[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, self, MAX_PATH) || !ParseVersion(ProductVersion(self), installed) || !ParseVersion(candidate, available)) return false;
    return CompareVersions(available, installed) > 0;
}

bool IsDigest(const std::wstring& digest) {
    if (digest.size() != 71 || digest.substr(0, 7) != L"sha256:") return false;
    for (size_t index = 7; index < digest.size(); ++index) {
        const wchar_t character = towlower(digest[index]);
        if (!((character >= L'0' && character <= L'9') || (character >= L'a' && character <= L'f'))) return false;
    }
    return true;
}

bool OpenRequest(const std::wstring& url, InternetHandle& session, InternetHandle& connection, InternetHandle& request) {
    URL_COMPONENTSW parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) return false;
    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.lpszExtraInfo && parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    session.value = WinHttpOpen(L"ClipTrace Updater/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session.value) return false;
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    if (!WinHttpSetOption(session.value, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy))) return false;
    WinHttpSetTimeouts(session.value, 5000, 5000, 10000, 20000);
    connection.value = WinHttpConnect(session.value, host.c_str(), parts.nPort, 0);
    if (!connection.value) return false;
    request.value = WinHttpOpenRequest(connection.value, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request.value) return false;
    const wchar_t headers[] = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
    if (!WinHttpSendRequest(request.value, headers, static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) return false;
    if (!WinHttpReceiveResponse(request.value, nullptr)) return false;
    DWORD status = 0;
    DWORD length = sizeof(status);
    return WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &length, nullptr) && status == 200;
}

bool ReadMetadata(std::string& result) {
    InternetHandle session, connection, request;
    if (!OpenRequest(ReleasesApi, session, connection, request)) return false;
    std::array<char, 16384> buffer{};
    DWORD read = 0;
    for (;;) {
        if (!WinHttpReadData(request.value, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) return false;
        if (!read) break;
        if (result.size() + read > 2 * 1024 * 1024) return false;
        result.append(buffer.data(), read);
    }
    return !result.empty();
}

std::unique_ptr<Release> FindRelease(const std::string& json) {
    try {
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, json.data(), static_cast<int>(json.size()), nullptr, 0);
        if (length <= 0) return nullptr;
        std::wstring wide(length, L'\0');
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, json.data(), static_cast<int>(json.size()), wide.data(), length)) return nullptr;
        const auto releases = winrt::Windows::Data::Json::JsonArray::Parse(wide);
        std::unique_ptr<Release> best;
        for (const auto& entry : releases) {
            const auto object = entry.GetObject();
            if (object.GetNamedBoolean(L"draft", false)) continue;
            const std::wstring tag = object.GetNamedString(L"tag_name", L"").c_str();
            if (!IsNewer(tag)) continue;
            if (best) {
                Version previous, candidate;
                if (!ParseVersion(best->tag, previous) || !ParseVersion(tag, candidate)) continue;
                if (CompareVersions(candidate, previous) <= 0) continue;
            }
            for (const auto& assetValue : object.GetNamedArray(L"assets")) {
                const auto asset = assetValue.GetObject();
                if (asset.GetNamedString(L"name", L"") != L"ClipTrace.exe") continue;
                const std::wstring url = asset.GetNamedString(L"browser_download_url", L"").c_str();
                const std::wstring digest = asset.GetNamedString(L"digest", L"").c_str();
                const double size = asset.GetNamedNumber(L"size", 0);
                if (url.rfind(std::wstring(AssetPrefix) + tag + L"/", 0) != 0 || !IsDigest(digest) || size < 1 || size > MaximumAssetSize) continue;
                best = std::make_unique<Release>(Release{tag, url, digest, static_cast<uint64_t>(size)});
                break;
            }
        }
        return best;
    } catch (...) {
        return nullptr;
    }
}

std::wstring UpdatesDirectory() {
    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) return L"";
    std::wstring root(local);
    CoTaskMemFree(local);
    root += L"\\ClipTrace";
    if (!CreateDirectoryW(root.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return L"";
    root += L"\\Updates";
    if (!CreateDirectoryW(root.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return L"";
    return root;
}

bool Sha256File(const std::wstring& path, std::wstring& result) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::array<BYTE, 32> digest{};
    bool okay = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
    std::array<BYTE, 65536> buffer{};
    DWORD read = 0;
    while (okay) {
        if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
            okay = false;
            break;
        }
        if (!read) break;
        okay = BCryptHashData(hash, buffer.data(), read, 0) >= 0;
    }
    if (okay) okay = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    if (!okay) return false;
    constexpr wchar_t hex[] = L"0123456789abcdef";
    result = L"sha256:";
    for (BYTE byte : digest) {
        result += hex[byte >> 4];
        result += hex[byte & 15];
    }
    return true;
}

bool Download(const Release& release, const std::wstring& path) {
    InternetHandle session, connection, request;
    if (!OpenRequest(release.url, session, connection, request)) return false;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    bool okay = true;
    uint64_t total = 0;
    std::array<BYTE, 65536> buffer{};
    DWORD read = 0;
    while (okay) {
        if (!WinHttpReadData(request.value, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) {
            okay = false;
            break;
        }
        if (!read) break;
        total += read;
        DWORD written = 0;
        okay = total <= release.size && WriteFile(file, buffer.data(), read, &written, nullptr) && written == read;
    }
    if (okay) okay = total == release.size && FlushFileBuffers(file);
    CloseHandle(file);
    std::wstring actual;
    if (okay) okay = Sha256File(path, actual) && _wcsicmp(actual.c_str(), release.digest.c_str()) == 0;
    DWORD type = 0;
    if (okay) okay = GetBinaryTypeW(path.c_str(), &type) && type == SCS_64BIT_BINARY && MatchesReleaseVersion(path, release.tag);
    if (!okay) DeleteFileW(path.c_str());
    return okay;
}

std::wstring Quote(const std::wstring& value) {
    return L"\"" + value + L"\"";
}

void PostError(HWND window, const std::wstring& message) {
    auto* detail = new std::wstring(message);
    if (!PostMessageW(window, ErrorMessage, 0, reinterpret_cast<LPARAM>(detail))) delete detail;
}

bool StartHelper(const std::wstring& staged, const Release& release) {
    const std::wstring directory = UpdatesDirectory();
    if (directory.empty()) return false;
    wchar_t original[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, original, MAX_PATH)) return false;
    const std::wstring helper = directory + L"\\ClipTraceUpdater.exe";
    if (!CopyFileW(original, helper.c_str(), FALSE)) return false;
    std::wstring command = Quote(helper) + L" --apply-update " + Quote(original) + L" " + Quote(staged) +
        L" " + std::to_wstring(GetCurrentProcessId()) + L" " + release.digest + L" " + release.tag;
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(helper.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) return false;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

bool IsExpectedTarget(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return _wcsicmp(path.substr(slash == std::wstring::npos ? 0 : slash + 1).c_str(), L"ClipTrace.exe") == 0;
}

void Relaunch(const std::wstring& target) {
    ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
}

void CheckAsync(HWND window, bool force) {
    const ULONGLONG now = GetTickCount64();
    if (!force && now - lastCheck.load() < 15ULL * 60ULL * 1000ULL) return;
    if (checking.exchange(true)) return;
    lastCheck = now;
    std::thread([window] {
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            std::string json;
            if (ReadMetadata(json)) {
                auto release = FindRelease(json);
                if (release && !PostMessageW(window, AvailableMessage, 0, reinterpret_cast<LPARAM>(release.get()))) {
                    release.reset();
                } else if (release) {
                    release.release();
                }
            }
            winrt::uninit_apartment();
        } catch (...) {}
        checking = false;
    }).detach();
}

bool InstallAsync(HWND window, Release release) {
    if (installing.exchange(true)) return false;
    std::thread([window, release = std::move(release)] {
        const std::wstring directory = UpdatesDirectory();
        const std::wstring staged = directory + L"\\ClipTrace-" + std::to_wstring(GetCurrentProcessId()) +
            L"-" + std::to_wstring(GetTickCount64()) + L".exe";
        bool ready = false;
        if (directory.empty() || !Download(release, staged)) {
            PostError(window, L"Não foi possível baixar ou verificar a atualização. Tente novamente mais tarde.");
        } else if (!StartHelper(staged, release)) {
            DeleteFileW(staged.c_str());
            PostError(window, L"Não foi possível preparar a instalação da atualização.");
        } else {
            ready = PostMessageW(window, ReadyMessage, 0, 0) != 0;
        }
        if (!ready) installing = false;
    }).detach();
    return true;
}

bool RunHelperIfRequested(int& exitCode) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return false;
    const bool requested = argc > 1 && wcscmp(argv[1], L"--apply-update") == 0;
    if (!requested) {
        LocalFree(argv);
        return false;
    }
    exitCode = 1;
    if (argc != 7) {
        LocalFree(argv);
        return true;
    }
    const std::wstring target = argv[2];
    const std::wstring staged = argv[3];
    const DWORD parentPid = wcstoul(argv[4], nullptr, 10);
    const std::wstring digest = argv[5];
    const std::wstring tag = argv[6];
    LocalFree(argv);
    Version releaseVersion;
    if (!IsExpectedTarget(target) || !IsDigest(digest) || !ParseVersion(tag, releaseVersion) || parentPid == 0) return true;
    const std::wstring directory = UpdatesDirectory();
    if (directory.empty() || staged.rfind(directory + L"\\", 0) != 0) return true;
    HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parentPid);
    if (parent) {
        const DWORD wait = WaitForSingleObject(parent, 30000);
        CloseHandle(parent);
        if (wait != WAIT_OBJECT_0) return true;
    }
    std::wstring actual;
    DWORD binaryType = 0;
    if (!Sha256File(staged, actual) || _wcsicmp(actual.c_str(), digest.c_str()) != 0 ||
        !GetBinaryTypeW(staged.c_str(), &binaryType) || binaryType != SCS_64BIT_BINARY || !MatchesReleaseVersion(staged, tag)) {
        DeleteFileW(staged.c_str());
        MessageBoxW(nullptr, L"O arquivo da atualização não passou na verificação de integridade.", L"ClipTrace", MB_OK | MB_ICONERROR);
        Relaunch(target);
        return true;
    }
    const std::wstring identity = std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64());
    const std::wstring replacement = target + L".new." + identity;
    const std::wstring backup = target + L".backup." + identity;
    bool installed = CopyFileW(staged.c_str(), replacement.c_str(), FALSE) != 0;
    if (installed) installed = Sha256File(replacement, actual) && _wcsicmp(actual.c_str(), digest.c_str()) == 0;
    if (installed) installed = ReplaceFileW(target.c_str(), replacement.c_str(), backup.c_str(), REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr) != 0;
    if (!installed) {
        DeleteFileW(replacement.c_str());
        DeleteFileW(staged.c_str());
        if (GetFileAttributesW(target.c_str()) == INVALID_FILE_ATTRIBUTES && GetFileAttributesW(backup.c_str()) != INVALID_FILE_ATTRIBUTES) {
            MoveFileExW(backup.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH);
        }
        MessageBoxW(nullptr, L"Não foi possível substituir o ClipTrace.exe. Verifique a permissão de escrita na pasta do programa.", L"ClipTrace", MB_OK | MB_ICONERROR);
        Relaunch(target);
        return true;
    }
    DeleteFileW(staged.c_str());
    Relaunch(target);
    exitCode = 0;
    return true;
}
}
