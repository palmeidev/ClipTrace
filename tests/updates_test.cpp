#include "../ClipTrace/updates.cpp"
#include <iostream>

bool InstallSmokeTest() {
    using namespace ClipTraceUpdates;
    wchar_t self[MAX_PATH]{}, temporary[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, self, MAX_PATH) || !GetTempPathW(MAX_PATH, temporary)) return false;
    const std::wstring directory = std::wstring(temporary) + L"ClipTraceUpdateTest-" + std::to_wstring(GetCurrentProcessId());
    if (!CreateDirectoryW(directory.c_str(), nullptr)) return false;
    const std::wstring target = directory + L"\\ClipTrace.exe";
    const std::wstring staged = UpdatesDirectory() + L"\\ClipTrace-smoke-" + std::to_wstring(GetCurrentProcessId()) + L".exe";
    std::wstring digest;
    bool okay = CopyFileW(self, target.c_str(), FALSE) && CopyFileW(self, staged.c_str(), FALSE) && Sha256File(staged, digest);
    PROCESS_INFORMATION held{};
    if (okay) {
        STARTUPINFOW startup{sizeof(startup)};
        std::wstring holdCommand = Quote(self) + L" --hold";
        okay = CreateProcessW(self, holdCommand.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &held) != 0;
    }
    if (okay) {
        std::wstring command = Quote(self) + L" --apply-update " + Quote(target) + L" " + Quote(staged) +
            L" " + std::to_wstring(held.dwProcessId) + L" " + digest + L" v1.1-preview";
        STARTUPINFOW startup{sizeof(startup)};
        PROCESS_INFORMATION process{};
        okay = CreateProcessW(self, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != 0;
        if (okay) {
            WaitForSingleObject(process.hProcess, 30000);
            DWORD code = 1;
            GetExitCodeProcess(process.hProcess, &code);
            okay = code == 0;
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
        }
    }
    if (held.hProcess) {
        WaitForSingleObject(held.hProcess, 5000);
        CloseHandle(held.hThread);
        CloseHandle(held.hProcess);
    }
    std::wstring installedDigest;
    if (okay) okay = Sha256File(target, installedDigest) && installedDigest == digest;
    for (int attempt = 0; attempt < 30 && !DeleteFileW(target.c_str()); ++attempt) Sleep(100);
    DeleteFileW(staged.c_str());
    WIN32_FIND_DATAW file{};
    HANDLE search = FindFirstFileW((target + L".backup.*").c_str(), &file);
    if (search != INVALID_HANDLE_VALUE) {
        do { DeleteFileW((directory + L"\\" + file.cFileName).c_str()); } while (FindNextFileW(search, &file));
        FindClose(search);
    }
    RemoveDirectoryW(directory.c_str());
    return okay;
}

int wmain(int argc, wchar_t** argv) {
    using namespace ClipTraceUpdates;
    int helperCode = 0;
    if (RunHelperIfRequested(helperCode)) return helperCode;
    if (argc > 1 && wcscmp(argv[1], L"--hold") == 0) {
        Sleep(600);
        return 0;
    }
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    Version installed, newer, older, previewTwo, previewTen;
    if (!ParseVersion(L"v1.1-preview", installed) || !ParseVersion(L"v1.2-preview", newer) ||
        !ParseVersion(L"v1.0-preview", older) || !ParseVersion(L"v1.1-preview.2", previewTwo) ||
        !ParseVersion(L"v1.1-preview.10", previewTen)) return 1;
    if (CompareVersions(newer, installed) <= 0 || CompareVersions(older, installed) >= 0 ||
        CompareVersions(previewTen, previewTwo) <= 0 || !IsDigest(L"sha256:677594227ed6c1b1dd56144424a5fdf21f37988e2a8194c81384172aa2c5d57d")) return 2;
    const std::string synthetic = R"([{"tag_name":"v1.2-preview","draft":false,"assets":[{"name":"ClipTrace.exe","browser_download_url":"https://github.com/palmeidev/ClipTrace/releases/download/v1.2-preview/ClipTrace.exe","digest":"sha256:677594227ed6c1b1dd56144424a5fdf21f37988e2a8194c81384172aa2c5d57d","size":454384}]}])";
    auto found = FindRelease(synthetic);
    if (!found || found->tag != L"v1.2-preview") return 3;
    if (argc > 1 && wcscmp(argv[1], L"--live") == 0) {
        std::string metadata;
        if (!ReadMetadata(metadata)) return 4;
        const auto releases = winrt::Windows::Data::Json::JsonArray::Parse(winrt::to_hstring(metadata));
        bool verified = false;
        for (const auto& entry : releases) {
            const auto release = entry.GetObject();
            if (release.GetNamedString(L"tag_name", L"") != L"v1.0-preview") continue;
            for (const auto& entryAsset : release.GetNamedArray(L"assets")) {
                const auto asset = entryAsset.GetObject();
                if (asset.GetNamedString(L"name", L"") != L"ClipTrace.exe") continue;
                Release current{L"v1.0-preview", asset.GetNamedString(L"browser_download_url", L"").c_str(),
                    asset.GetNamedString(L"digest", L"").c_str(), static_cast<uint64_t>(asset.GetNamedNumber(L"size", 0))};
                const std::wstring temp = UpdatesDirectory() + L"\\ClipTrace-test-" + std::to_wstring(GetCurrentProcessId()) + L".exe";
                verified = IsDigest(current.digest) && Download(current, temp);
                DeleteFileW(temp.c_str());
                if (verified) {
                    current.digest = L"sha256:0000000000000000000000000000000000000000000000000000000000000000";
                    verified = !Download(current, temp) && GetFileAttributesW(temp.c_str()) == INVALID_FILE_ATTRIBUTES;
                }
                break;
            }
        }
        if (!verified) return 5;
    }
    if (argc > 1 && wcscmp(argv[1], L"--install-smoke") == 0 && !InstallSmokeTest()) return 6;
    std::wcout << L"OK\n";
    return 0;
}
