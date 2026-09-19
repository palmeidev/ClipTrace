#define wWinMain ClipTraceApplicationMain
#include "../ClipTrace/main.cpp"
#undef wWinMain

int wmain() {
    wchar_t temporary[MAX_PATH]{};
    if (!GetTempPathW(MAX_PATH, temporary)) return 1;
    const std::wstring root = std::wstring(temporary) + L"ClipTraceHistoryTest-" + std::to_wstring(GetCurrentProcessId());
    if (!CreateDirectoryW(root.c_str(), nullptr) || !SetEnvironmentVariableW(L"APPDATA", root.c_str())) return 2;

    const DWORD pixels[4]{0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFFFFFFFF};
    HBITMAP bitmap = CreateBitmap(2, 2, 1, 32, pixels);
    if (!bitmap) return 3;
    ClipItem capture{};
    capture.type = ClipItem::Type::Image;
    capture.preview = L"Captura de tela: Mesmo título";
    capture.rawContent = L"Mesmo aplicativo";
    capture.source = L"Mesmo aplicativo";
    capture.pageTitle = L"Mesmo título";
    capture.image = std::shared_ptr<ClipItem::BitmapObject>(bitmap, [](ClipItem::BitmapObject* value) { DeleteObject(value); });

    const DWORD changedPixels[4]{0xFF0000FE, 0xFF00FF00, 0xFFFF0000, 0xFFFFFFFF};
    HBITMAP changedBitmap = CreateBitmap(2, 2, 1, 32, changedPixels);
    if (!changedBitmap) return 5;
    const bool coalescingOkay = !IsRepeatedClipboardImage(bitmap, 1000) &&
        IsRepeatedClipboardImage(bitmap, 1100) &&
        !IsRepeatedClipboardImage(bitmap, 1500) &&
        !IsRepeatedClipboardImage(changedBitmap, 1600);
    DeleteObject(changedBitmap);

    AddClipboardItem(capture);
    AddClipboardItem(capture);
    AddClipboardItem(capture);
    const std::wstring directory = GetAppDataDirectory();
    const bool okay = gHistory.size() == 3 && !gHistory[0].imageFile.empty() &&
        !gHistory[1].imageFile.empty() && !gHistory[2].imageFile.empty() &&
        gHistory[0].imageFile != gHistory[1].imageFile &&
        gHistory[0].imageFile != gHistory[2].imageFile &&
        gHistory[1].imageFile != gHistory[2].imageFile &&
        GetFileAttributesW((directory + L"\\images\\" + gHistory[0].imageFile).c_str()) != INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesW((directory + L"\\images\\" + gHistory[1].imageFile).c_str()) != INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesW((directory + L"\\images\\" + gHistory[2].imageFile).c_str()) != INVALID_FILE_ATTRIBUTES;
    for (const auto& item : gHistory) DeleteFileW((directory + L"\\images\\" + item.imageFile).c_str());
    gHistory.clear();
    DeleteFileW((directory + L"\\history.dat").c_str());
    RemoveDirectoryW((directory + L"\\images").c_str());
    RemoveDirectoryW(directory.c_str());
    RemoveDirectoryW(root.c_str());
    return okay && coalescingOkay ? 0 : 4;
}
