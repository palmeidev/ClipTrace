#pragma once

#include <windows.h>

// Biblioteca de icones padrao do Windows 11 (Segoe Fluent Icons / GDI).
// Utiliza os glifos nativos do Windows 11 para fidelidade visual com o sistema operacional.
namespace ClipTraceIcons {
// Pontos de codigo Unicode dos icones padrao do Windows 11 (Segoe Fluent Icons)
constexpr const wchar_t* kClose = L"\xE8BB";           // ChromeClose (icone X de fechar do Windows 11)
constexpr const wchar_t* kCancel = L"\xE711";          // Cancel / Fechar simples
constexpr const wchar_t* kEmptyClipboard = L"\xF0E3";  // ClipboardList (icone de area de transferencia do Win11)
constexpr const wchar_t* kPaste = L"\xE77F";           // Paste
constexpr const wchar_t* kMore = L"\xE712";            // Mais opcoes (...)
constexpr const wchar_t* kPin = L"\xE718";             // Fixar
constexpr const wchar_t* kUnpin = L"\xE77A";           // Desafixar
constexpr const wchar_t* kCopy = L"\xE8C8";            // Copiar
constexpr const wchar_t* kInfo = L"\xE946";            // Informacoes
constexpr const wchar_t* kSettings = L"\xE713";        // Configuracoes (engrenagem)
constexpr const wchar_t* kDelete = L"\xE74D";          // Apagar / Lixeira
constexpr const wchar_t* kCheckMark = L"\xE73E";       // CheckMark (sucesso)
constexpr const wchar_t* kSearch = L"\xE721";          // Lupa / Pesquisar
constexpr const wchar_t* kPause = L"\xE769";           // Pausar monitoramento
constexpr const wchar_t* kPlay = L"\xE768";            // Retomar monitoramento
constexpr const wchar_t* kColor = L"\xE790";           // Paleta de cores
constexpr const wchar_t* kShield = L"\xE72E";          // Escudo / Protecao de Senhas
constexpr const wchar_t* kGithub = L"\xE71B";          // Link / Repositorio GitHub
constexpr const wchar_t* kHeart = L"\xEB51";           // Coracao / Open Source

inline void Line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width = 1) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr); LineTo(dc, x2, y2);
    SelectObject(dc, oldPen); DeleteObject(pen);
}

inline void DrawClose(HDC dc, RECT bounds, HFONT font, COLORREF color) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    HGDIOBJ oldFont = SelectObject(dc, font);
    DrawTextW(dc, kClose, -1, &bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, oldFont);
}

inline void DrawEmptyClipboard(HDC dc, RECT bounds, HFONT font, COLORREF color) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    HGDIOBJ oldFont = SelectObject(dc, font);
    DrawTextW(dc, kEmptyClipboard, -1, &bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, oldFont);
}

// Implementacao GDI de fallback
inline void DrawCloseGdi(HDC dc, RECT bounds, COLORREF color) {
    const int cx = (bounds.left + bounds.right) / 2;
    const int cy = (bounds.top + bounds.bottom) / 2;
    Line(dc, cx - 5, cy - 5, cx + 5, cy + 5, color);
    Line(dc, cx + 5, cy - 5, cx - 5, cy + 5, color);
}

inline void DrawOverflow(HDC dc, RECT bounds, COLORREF color) {
    const int cy = (bounds.top + bounds.bottom) / 2;
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    for (int x = bounds.left + 5; x <= bounds.right - 5; x += 6)
        Ellipse(dc, x - 1, cy - 1, x + 2, cy + 2);
    SelectObject(dc, oldBrush);
    DeleteObject(brush);
}

inline void DrawPin(HDC dc, RECT bounds, COLORREF color) {
    const int cx = (bounds.left + bounds.right) / 2;
    const int top = bounds.top + 2;
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, cx - 4, top, cx + 4, top + 6);
    Line(dc, cx, top + 6, cx, top + 12, color);
    Line(dc, cx - 3, top + 12, cx + 3, top + 12, color);
    SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(pen);
}

inline void DrawDetails(HDC dc, RECT bounds, COLORREF color) {
    const int cx = (bounds.left + bounds.right) / 2;
    const int cy = (bounds.top + bounds.bottom) / 2;
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, cx - 7, cy - 7, cx + 7, cy + 7);
    SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(pen);
    HBRUSH dot = CreateSolidBrush(color); oldBrush = SelectObject(dc, dot);
    Ellipse(dc, cx - 1, cy - 4, cx + 2, cy - 1);
    SelectObject(dc, oldBrush); DeleteObject(dot);
    Line(dc, cx, cy, cx, cy + 5, color, 1);
}

inline void DrawCopy(HDC dc, RECT bounds, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, bounds.left + 5, bounds.top + 3, bounds.right - 3, bounds.bottom - 5);
    Rectangle(dc, bounds.left + 2, bounds.top + 6, bounds.right - 6, bounds.bottom - 2);
    SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(pen);
}
}
