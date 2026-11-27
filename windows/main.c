/*
 * AutoClicker Multi-Point / Multi-Ponto for Windows (Adwaita Dark Red UI)
 * Native Win32 64-bit with:
 * - Full Per-Monitor DPI scaling (100%, 125%, 150%, 200%)
 * - Full horizontal & vertical resizing (WS_OVERLAPPEDWINDOW + WM_SIZE)
 * - Automatic vertical scrolling (WM_MOUSEWHEEL + WM_VSCROLL) when window is reduced
 * - Dark titlebar (Windows 10/11 DWM), rounded grouped cards, steppers, presets, EN/PT toggle
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#ifndef GET_WHEEL_DELTA_WPARAM
#define GET_WHEEL_DELTA_WPARAM(wParam) ((short)HIWORD(wParam))
#endif

#define IDI_APP_ICON 101
#define TIMER_TICK   1001
#define TIMER_TOAST  1002

#ifdef DEFAULT_LANG_EN
#define INIT_LANG 1
#else
#define INIT_LANG 0
#endif

// Paleta de Cores fiel ao GTK4 / Libadwaita Dark + Red Accent
#define CLR_BG            RGB(36, 36, 36)     // #242424 Fundo principal
#define CLR_HEADER        RGB(42, 42, 42)     // #2A2A2A Header bar
#define CLR_CARD          RGB(46, 46, 46)     // #2E2E2E Card principal
#define CLR_ROW           RGB(56, 56, 56)     // #383838 Linhas agrupadas
#define CLR_DIVIDER       RGB(68, 68, 68)     // #444444 Divisorias sutis
#define CLR_CARD_BORDER   RGB(60, 60, 60)     // #3C3C3C Borda card

#define CLR_TEXT_PRIMARY  RGB(250, 250, 250)  // #FAFAFA Branco limpo
#define CLR_TEXT_MUTED    RGB(162, 162, 162)  // #A2A2A2 Cinza claro
#define CLR_TEXT_SUB      RGB(140, 140, 140)  // #8C8C8C Subtitulos

#define CLR_ACCENT        RGB(224, 52, 74)    // #E0344A Vermelho Adwaita
#define CLR_ACCENT_PRESS  RGB(190, 38, 58)    // #BE263A Vermelho pressionado
#define CLR_STOP_DARK     RGB(178, 32, 48)    // #B22030 Parar
#define CLR_GREEN         RGB(46, 194, 126)   // #2EC27E Toast salvo

#define CLR_BTN_DARK      RGB(68, 68, 68)     // #444444 Botoes secundarios / steppers
#define CLR_BTN_DARK_PRS  RGB(84, 84, 84)     // #545454 Pressionado
#define CLR_TAB_BG        RGB(48, 48, 48)     // #303030 Fundo barra de abas
#define CLR_TAB_ACTIVE    RGB(66, 66, 66)     // #424242 Aba ativa
#define CLR_TRACK_BG      RGB(30, 30, 30)     // #1E1E1E Trilha barra progresso
#define CLR_SWITCH_OFF    RGB(86, 86, 86)     // #565656 Switch desligado
#define CLR_KNOB          RGB(252, 252, 252)  // #FCFCFC Bolinha switch

// Dimensoes base (em 96 DPI)
#define BASE_CLIENT_W     580
#define BASE_CLIENT_H     744
#define MIN_CLIENT_W      460
#define MIN_CLIENT_H      340

// IDs de controles
#define IDC_BTN_SAVE         2001
#define IDC_BTN_TOGGLE       2002
#define IDC_TAB_P1           2003
#define IDC_TAB_P2           2004
#define IDC_BTN_LANG         2005

#define IDC_SW_ENABLED       2101
#define IDC_SW_FIXED         2102
#define IDC_BTN_PICK         2103
#define IDC_EDT_X            2104
#define IDC_BTN_X_DEC        2105
#define IDC_BTN_X_INC        2106
#define IDC_EDT_Y            2107
#define IDC_BTN_Y_DEC        2108
#define IDC_BTN_Y_INC        2109
#define IDC_EDT_MIN          2110
#define IDC_BTN_MIN_DEC      2111
#define IDC_BTN_MIN_INC      2112
#define IDC_EDT_SEC          2113
#define IDC_BTN_SEC_DEC      2114
#define IDC_BTN_SEC_INC      2115

#define IDC_PRESET_5S        2120
#define IDC_PRESET_10S       2121
#define IDC_PRESET_30S       2122
#define IDC_PRESET_1M        2123
#define IDC_PRESET_2M        2124
#define IDC_PRESET_5M        2125
#define IDC_PRESET_10M       2126

#define IDC_BTN_MBTN         2130
#define IDC_SW_DBL           2131
#define IDC_BTN_TEST         2132

typedef struct {
    BOOL enabled;
    BOOL use_fixed;
    int x;
    int y;
    int minutes;
    int seconds;
    int button; // 0=Left, 1=Right, 2=Middle
    BOOL double_click;
    DWORD next_tick;
    DWORD interval_ms;
    int total_clicks;
} TargetConfig;

static TargetConfig g_targets[2] = {
    { TRUE, TRUE, 500, 400, 2, 0, 0, FALSE, 0, 120000, 0 },
    { TRUE, TRUE, 800, 600, 5, 0, 0, FALSE, 0, 300000, 0 }
};

static int g_dpi = 96;
static int g_scroll_y = 0;
static int g_total_content_h = 744;
static int g_lang = INIT_LANG; // 0 = PT-BR, 1 = EN
static int g_active_tab = 0;   // 0 = Point 1, 1 = Point 2
static BOOL g_is_running = FALSE;
static BOOL g_play_sound = TRUE;
static BOOL g_show_saved_toast = FALSE;
static BOOL g_updating_ui = FALSE;
static int g_picking_target = -1;

static HWND g_hMainWnd = NULL;

static HFONT g_hFontHdrTitle  = NULL;
static HFONT g_hFontHdrSub    = NULL;
static HFONT g_hFontCardTitle = NULL;
static HFONT g_hFontTimer     = NULL;
static HFONT g_hFontSection   = NULL;
static HFONT g_hFontBtnBig    = NULL;
static HFONT g_hFontUi        = NULL;
static HFONT g_hFontUiBold    = NULL;
static HFONT g_hFontSmall     = NULL;

static HBRUSH g_brBg     = NULL;
static HBRUSH g_brHeader = NULL;
static HBRUSH g_brCard   = NULL;
static HBRUSH g_brRow    = NULL;

static char g_ini_path[MAX_PATH] = {0};

static int S(int px) {
    return MulDiv(px, g_dpi, 96);
}

static const char *T(const char *pt, const char *en) {
    return (g_lang == 1) ? en : pt;
}

static TargetConfig *CurTarget(void) {
    return &g_targets[g_active_tab];
}

static void RecalcInterval(TargetConfig *t) {
    if (t->minutes < 0) t->minutes = 0;
    if (t->minutes > 999) t->minutes = 999;
    if (t->seconds < 0) t->seconds = 0;
    if (t->seconds > 59) t->seconds = 59;
    DWORD ms = (DWORD)((t->minutes * 60 + t->seconds) * 1000);
    if (ms < 200) ms = 200;
    t->interval_ms = ms;
}

static const char *GetMouseBtnLabel(int btn) {
    if (btn == 1) return T("Direito  v", "Right  v");
    if (btn == 2) return T("Meio  v", "Middle  v");
    return T("Esquerdo  v", "Left  v");
}

static void DestroyFonts(void) {
    if (g_hFontHdrTitle)  DeleteObject(g_hFontHdrTitle);
    if (g_hFontHdrSub)    DeleteObject(g_hFontHdrSub);
    if (g_hFontCardTitle) DeleteObject(g_hFontCardTitle);
    if (g_hFontTimer)     DeleteObject(g_hFontTimer);
    if (g_hFontSection)   DeleteObject(g_hFontSection);
    if (g_hFontBtnBig)    DeleteObject(g_hFontBtnBig);
    if (g_hFontUi)        DeleteObject(g_hFontUi);
    if (g_hFontUiBold)    DeleteObject(g_hFontUiBold);
    if (g_hFontSmall)     DeleteObject(g_hFontSmall);
}

static void CreateScaledFonts(void) {
    DestroyFonts();
    g_hFontHdrTitle  = CreateFontA(-S(14), 0, 0, 0, FW_BOLD,     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_hFontHdrSub    = CreateFontA(-S(11), 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_hFontCardTitle = CreateFontA(-S(16), 0, 0, 0, FW_BOLD,     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_hFontTimer     = CreateFontA(-S(30), 0, 0, 0, FW_BOLD,     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_hFontSection   = CreateFontA(-S(13), 0, 0, 0, FW_BOLD,     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_hFontBtnBig    = CreateFontA(-S(14), 0, 0, 0, FW_BOLD,     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_hFontUi        = CreateFontA(-S(13), 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_hFontUiBold    = CreateFontA(-S(13), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_hFontSmall     = CreateFontA(-S(11), 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
}

static void ApplyFontsToEdits(HWND hwnd) {
    int edits[4] = { IDC_EDT_X, IDC_EDT_Y, IDC_EDT_MIN, IDC_EDT_SEC };
    for (int i = 0; i < 4; i++) {
        HWND h = GetDlgItem(hwnd, edits[i]);
        if (h) SendMessageA(h, WM_SETFONT, (WPARAM)g_hFontUiBold, TRUE);
    }
}

static void InitIniPath(void) {
    char appdata[MAX_PATH];
    if (GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH) > 0) {
        wsprintfA(g_ini_path, "%s\\AutoClicker", appdata);
        CreateDirectoryA(g_ini_path, NULL);
        wsprintfA(g_ini_path, "%s\\AutoClicker\\config.ini", appdata);
    } else {
        lstrcpyA(g_ini_path, ".\\autoclicker_config.ini");
    }
}

static void LoadConfig(void) {
    InitIniPath();
    g_play_sound = GetPrivateProfileIntA("General", "PlaySound", 1, g_ini_path);
    g_lang       = GetPrivateProfileIntA("General", "Language", INIT_LANG, g_ini_path);
    if (g_lang != 0 && g_lang != 1) g_lang = INIT_LANG;

    const char *secs[2] = { "Point1", "Point2" };
    int defX[2] = { 500, 800 };
    int defY[2] = { 400, 600 };
    int defM[2] = { 2, 5 };

    for (int i = 0; i < 2; i++) {
        TargetConfig *t = &g_targets[i];
        t->enabled      = GetPrivateProfileIntA(secs[i], "Enabled", 1, g_ini_path);
        t->use_fixed    = GetPrivateProfileIntA(secs[i], "UseFixed", 1, g_ini_path);
        t->x            = (int)GetPrivateProfileIntA(secs[i], "X", defX[i], g_ini_path);
        t->y            = (int)GetPrivateProfileIntA(secs[i], "Y", defY[i], g_ini_path);
        t->minutes      = GetPrivateProfileIntA(secs[i], "Minutes", defM[i], g_ini_path);
        t->seconds      = GetPrivateProfileIntA(secs[i], "Seconds", 0, g_ini_path);
        t->button       = GetPrivateProfileIntA(secs[i], "Button", 0, g_ini_path);
        t->double_click = GetPrivateProfileIntA(secs[i], "DoubleClick", 0, g_ini_path);
        RecalcInterval(t);
    }
}

static void SaveConfig(void) {
    char buf[32];
    wsprintfA(buf, "%d", g_play_sound);
    WritePrivateProfileStringA("General", "PlaySound", buf, g_ini_path);
    wsprintfA(buf, "%d", g_lang);
    WritePrivateProfileStringA("General", "Language", buf, g_ini_path);

    const char *secs[2] = { "Point1", "Point2" };
    for (int i = 0; i < 2; i++) {
        TargetConfig *t = &g_targets[i];
        wsprintfA(buf, "%d", t->enabled);      WritePrivateProfileStringA(secs[i], "Enabled", buf, g_ini_path);
        wsprintfA(buf, "%d", t->use_fixed);    WritePrivateProfileStringA(secs[i], "UseFixed", buf, g_ini_path);
        wsprintfA(buf, "%d", t->x);            WritePrivateProfileStringA(secs[i], "X", buf, g_ini_path);
        wsprintfA(buf, "%d", t->y);            WritePrivateProfileStringA(secs[i], "Y", buf, g_ini_path);
        wsprintfA(buf, "%d", t->minutes);      WritePrivateProfileStringA(secs[i], "Minutes", buf, g_ini_path);
        wsprintfA(buf, "%d", t->seconds);      WritePrivateProfileStringA(secs[i], "Seconds", buf, g_ini_path);
        wsprintfA(buf, "%d", t->button);       WritePrivateProfileStringA(secs[i], "Button", buf, g_ini_path);
        wsprintfA(buf, "%d", t->double_click); WritePrivateProfileStringA(secs[i], "DoubleClick", buf, g_ini_path);
    }
}

static void DoMouseAction(DWORD flags) {
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    SendInput(1, &input, sizeof(INPUT));
}

static void ExecuteClick(TargetConfig *t) {
    if (t->use_fixed) {
        SetCursorPos(t->x, t->y);
        Sleep(20);
    }

    DWORD down_flag = MOUSEEVENTF_LEFTDOWN;
    DWORD up_flag   = MOUSEEVENTF_LEFTUP;
    if (t->button == 1) {
        down_flag = MOUSEEVENTF_RIGHTDOWN;
        up_flag   = MOUSEEVENTF_RIGHTUP;
    } else if (t->button == 2) {
        down_flag = MOUSEEVENTF_MIDDLEDOWN;
        up_flag   = MOUSEEVENTF_MIDDLEUP;
    }

    int loops = t->double_click ? 2 : 1;
    for (int i = 0; i < loops; i++) {
        DoMouseAction(down_flag);
        Sleep(35);
        DoMouseAction(up_flag);
        if (i < loops - 1) Sleep(60);
    }

    t->total_clicks++;
    if (g_play_sound) {
        MessageBeep(MB_OK);
    }
}

static void UpdateEditBoxesFromTarget(void) {
    if (!g_hMainWnd) return;
    g_updating_ui = TRUE;
    TargetConfig *t = CurTarget();
    SetDlgItemInt(g_hMainWnd, IDC_EDT_X, t->x, TRUE);
    SetDlgItemInt(g_hMainWnd, IDC_EDT_Y, t->y, TRUE);
    SetDlgItemInt(g_hMainWnd, IDC_EDT_MIN, t->minutes, FALSE);
    SetDlgItemInt(g_hMainWnd, IDC_EDT_SEC, t->seconds, FALSE);
    g_updating_ui = FALSE;
}

static void SyncEditBoxesToTarget(void) {
    if (!g_hMainWnd || g_updating_ui) return;
    TargetConfig *t = CurTarget();
    t->x       = (int)GetDlgItemInt(g_hMainWnd, IDC_EDT_X, NULL, TRUE);
    t->y       = (int)GetDlgItemInt(g_hMainWnd, IDC_EDT_Y, NULL, TRUE);
    t->minutes = (int)GetDlgItemInt(g_hMainWnd, IDC_EDT_MIN, NULL, FALSE);
    t->seconds = (int)GetDlgItemInt(g_hMainWnd, IDC_EDT_SEC, NULL, FALSE);
    RecalcInterval(t);
}

static void RedrawAll(void) {
    if (!g_hMainWnd) return;
    SetWindowTextA(g_hMainWnd, T("AutoClicker Multi-Ponto", "AutoClicker Multi-Point"));
    InvalidateRect(g_hMainWnd, NULL, FALSE);
    int ids[] = {
        IDC_BTN_SAVE, IDC_BTN_LANG, IDC_BTN_TOGGLE, IDC_TAB_P1, IDC_TAB_P2,
        IDC_SW_ENABLED, IDC_SW_FIXED, IDC_BTN_PICK,
        IDC_BTN_X_DEC, IDC_BTN_X_INC, IDC_BTN_Y_DEC, IDC_BTN_Y_INC,
        IDC_BTN_MIN_DEC, IDC_BTN_MIN_INC, IDC_BTN_SEC_DEC, IDC_BTN_SEC_INC,
        IDC_PRESET_5S, IDC_PRESET_10S, IDC_PRESET_30S, IDC_PRESET_1M,
        IDC_PRESET_2M, IDC_PRESET_5M, IDC_PRESET_10M,
        IDC_BTN_MBTN, IDC_SW_DBL, IDC_BTN_TEST
    };
    for (int i = 0; i < (int)(sizeof(ids)/sizeof(ids[0])); i++) {
        HWND h = GetDlgItem(g_hMainWnd, ids[i]);
        if (h) InvalidateRect(h, NULL, FALSE);
    }
}

// Calcula a altura total do conteudo em pixels atuais (DPI-scaled)
typedef struct {
    int pad;
    int hdrH;
    int yBtn;
    int yTabs;
    int cy;
    int g1y;
    int rH1;
    int g2y;
    int rH2;
    int py;
    int g3y;
    int rH3;
    int yTest;
    int chCard;
    int totalH;
} LayoutMetrics;

static void ComputeMetrics(LayoutMetrics *m) {
    m->pad   = S(16);
    m->hdrH  = S(46);
    m->yBtn  = m->hdrH + S(10);
    m->yTabs = m->yBtn + S(48);
    m->cy    = m->yTabs + S(42);
    m->rH1   = S(40);
    m->rH2   = S(36);
    m->rH3   = S(36);
    m->g1y   = m->cy + S(126);
    m->g2y   = m->g1y + m->rH1 * 4 + S(28);
    m->py    = m->g2y + m->rH2 * 2 + S(8);
    m->g3y   = m->py + S(54);
    m->yTest = m->g3y + m->rH3 * 2 + S(10);
    m->chCard = (m->yTest + S(34) + S(14)) - m->cy;
    m->totalH = m->cy + m->chCard + m->pad;
}

static void UpdateScrollInfo(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int ch = rc.bottom - rc.top;

    LayoutMetrics m;
    ComputeMetrics(&m);
    g_total_content_h = m.totalH;

    int maxScroll = g_total_content_h - ch;
    if (maxScroll < 0) maxScroll = 0;
    if (g_scroll_y > maxScroll) g_scroll_y = maxScroll;
    if (g_scroll_y < 0) g_scroll_y = 0;

    SCROLLINFO si = {0};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin   = 0;
    if (maxScroll == 0) {
        si.nMax  = 0;
        si.nPage = (UINT)(ch + 1);
        si.nPos  = 0;
    } else {
        si.nMax  = g_total_content_h - 1;
        si.nPage = (UINT)ch;
        si.nPos  = g_scroll_y;
    }
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

static void LayoutControls(HWND hwnd) {
    UpdateScrollInfo(hwnd);

    RECT rc;
    GetClientRect(hwnd, &rc);
    int cw = rc.right - rc.left;
    if (cw < S(260)) return;

    LayoutMetrics m;
    ComputeMetrics(&m);

    int oy = -g_scroll_y;
    int contentW = cw - m.pad * 2;

    // Header buttons
    MoveWindow(GetDlgItem(hwnd, IDC_BTN_SAVE), S(12), oy + S(8), S(72), S(30), TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_BTN_LANG), cw - S(58), oy + S(8), S(46), S(30), TRUE);

    // Big Red Start/Stop Button
    MoveWindow(GetDlgItem(hwnd, IDC_BTN_TOGGLE), m.pad, oy + m.yBtn, contentW, S(40), TRUE);

    // Tabs P1 / P2
    int tabGap = S(8);
    int tabW = (contentW - tabGap) / 2;
    MoveWindow(GetDlgItem(hwnd, IDC_TAB_P1), m.pad, oy + m.yTabs, tabW, S(34), TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_TAB_P2), m.pad + tabW + tabGap, oy + m.yTabs, contentW - tabW - tabGap, S(34), TRUE);

    int sx = m.pad + S(14);
    int sw = contentW - S(28);
    int rightEdge = sx + sw;

    // Enable Switch
    MoveWindow(GetDlgItem(hwnd, IDC_SW_ENABLED), rightEdge - S(48), oy + m.cy + S(10), S(48), S(26), TRUE);

    // Section 1: Click Location
    MoveWindow(GetDlgItem(hwnd, IDC_SW_FIXED), rightEdge - S(58), oy + m.g1y + S(7), S(48), S(26), TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_BTN_PICK), rightEdge - S(156), oy + m.g1y + m.rH1 + S(4), S(144), S(32), TRUE);

    int stSize = S(26);
    int stIncX = rightEdge - S(10) - stSize;
    int stDecX = stIncX - S(6) - stSize;
    int edtW   = S(64);
    int edtH   = S(20);
    int edtX   = stDecX - S(8) - edtW;

    int r3y = oy + m.g1y + m.rH1 * 2;
    MoveWindow(GetDlgItem(hwnd, IDC_EDT_X),     edtX,   r3y + (m.rH1 - edtH) / 2,   edtW,   edtH,   TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_BTN_X_DEC), stDecX, r3y + (m.rH1 - stSize) / 2, stSize, stSize, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_BTN_X_INC), stIncX, r3y + (m.rH1 - stSize) / 2, stSize, stSize, TRUE);

    int r4y = oy + m.g1y + m.rH1 * 3;
    MoveWindow(GetDlgItem(hwnd, IDC_EDT_Y),     edtX,   r4y + (m.rH1 - edtH) / 2,   edtW,   edtH,   TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_BTN_Y_DEC), stDecX, r4y + (m.rH1 - stSize) / 2, stSize, stSize, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_BTN_Y_INC), stIncX, r4y + (m.rH1 - stSize) / 2, stSize, stSize, TRUE);

    // Section 2: Time Interval
    int r5y = oy + m.g2y;
    MoveWindow(GetDlgItem(hwnd, IDC_EDT_MIN),     edtX,   r5y + (m.rH2 - edtH) / 2,   edtW,   edtH,   TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_BTN_MIN_DEC), stDecX, r5y + (m.rH2 - stSize) / 2, stSize, stSize, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_BTN_MIN_INC), stIncX, r5y + (m.rH2 - stSize) / 2, stSize, stSize, TRUE);

    int r6y = oy + m.g2y + m.rH2;
    MoveWindow(GetDlgItem(hwnd, IDC_EDT_SEC),     edtX,   r6y + (m.rH2 - edtH) / 2,   edtW,   edtH,   TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_BTN_SEC_DEC), stDecX, r6y + (m.rH2 - stSize) / 2, stSize, stSize, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_BTN_SEC_INC), stIncX, r6y + (m.rH2 - stSize) / 2, stSize, stSize, TRUE);

    // Presets
    int pGap = S(4);
    int pW = (sw - pGap * 6) / 7;
    for (int i = 0; i < 7; i++) {
        MoveWindow(GetDlgItem(hwnd, IDC_PRESET_5S + i), sx + i * (pW + pGap), oy + m.py, pW, S(26), TRUE);
    }

    // Section 3: Button Options
    MoveWindow(GetDlgItem(hwnd, IDC_BTN_MBTN), rightEdge - S(124), oy + m.g3y + S(4),         S(112), S(28), TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_SW_DBL),   rightEdge - S(58),  oy + m.g3y + m.rH3 + S(5), S(48),  S(26), TRUE);

    // Test Click Button
    MoveWindow(GetDlgItem(hwnd, IDC_BTN_TEST), sx, oy + m.yTest, sw, S(34), TRUE);
}

static void SetScrollPosAndRefresh(HWND hwnd, int newPos) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int ch = rc.bottom - rc.top;
    int maxScroll = g_total_content_h - ch;
    if (maxScroll < 0) maxScroll = 0;
    if (newPos < 0) newPos = 0;
    if (newPos > maxScroll) newPos = maxScroll;
    if (newPos != g_scroll_y) {
        g_scroll_y = newPos;
        LayoutControls(hwnd);
        RedrawAll();
    }
}

static void ToggleRunning(void) {
    SyncEditBoxesToTarget();
    g_is_running = !g_is_running;

    if (g_is_running) {
        DWORD now = GetTickCount();
        for (int i = 0; i < 2; i++) {
            RecalcInterval(&g_targets[i]);
            g_targets[i].next_tick = now + g_targets[i].interval_ms;
        }
        SetTimer(g_hMainWnd, TIMER_TICK, 100, NULL);
    } else {
        KillTimer(g_hMainWnd, TIMER_TICK);
    }
    RedrawAll();
}

static void StartCapture(int target_idx) {
    g_picking_target = target_idx;
    SetCapture(g_hMainWnd);
    SetCursor(LoadCursorA(NULL, IDC_CROSS));
    InvalidateRect(GetDlgItem(g_hMainWnd, IDC_BTN_PICK), NULL, FALSE);
}

static void CancelCapture(void) {
    if (g_picking_target >= 0) {
        g_picking_target = -1;
        ReleaseCapture();
        SetCursor(LoadCursorA(NULL, IDC_ARROW));
        InvalidateRect(GetDlgItem(g_hMainWnd, IDC_BTN_PICK), NULL, FALSE);
    }
}

static void FinishCapture(int x, int y) {
    if (g_picking_target >= 0 && g_picking_target < 2) {
        g_targets[g_picking_target].x = x;
        g_targets[g_picking_target].y = y;
        g_targets[g_picking_target].use_fixed = TRUE;
    }
    CancelCapture();
    UpdateEditBoxesFromTarget();
    SaveConfig();
    RedrawAll();
}

// Helpers GDI
static void FillRoundRect(HDC hdc, int x, int y, int w, int h, int radius, COLORREF fillClr, COLORREF borderClr) {
    HBRUSH hBr = CreateSolidBrush(fillClr);
    HPEN hPen = CreatePen(PS_SOLID, 1, borderClr);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, hBr);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

    RoundRect(hdc, x, y, x + w, y + h, radius, radius);

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);
}

static void DrawHLine(HDC hdc, int x1, int x2, int y, COLORREF clr) {
    HPEN hPen = CreatePen(PS_SOLID, 1, clr);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, x1, y, NULL);
    LineTo(hdc, x2, y);
    SelectObject(hdc, oldPen);
    DeleteObject(hPen);
}

static void FormatCountdown(TargetConfig *t, char *outBuf) {
    DWORD rem_ms = t->interval_ms;
    if (g_is_running && t->enabled) {
        DWORD now = GetTickCount();
        rem_ms = ((LONG)(t->next_tick - now) > 0) ? (t->next_tick - now) : 0;
    }
    int total_sec = (int)((rem_ms + 999) / 1000);
    int mins = total_sec / 60;
    int secs = total_sec % 60;
    wsprintfA(outBuf, "%02d:%02d", mins, secs);
}

static void PaintMainWindow(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdcScreen = BeginPaint(hwnd, &ps);

    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    int cw = rcClient.right - rcClient.left;
    int ch = rcClient.bottom - rcClient.top;
    if (cw <= 0 || ch <= 0) {
        EndPaint(hwnd, &ps);
        return;
    }

    HDC hdc = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, cw, ch);
    HBITMAP oldBmp = (HBITMAP)SelectObject(hdc, hBmp);

    LayoutMetrics m;
    ComputeMetrics(&m);
    int oy = -g_scroll_y;
    int contentW = cw - m.pad * 2;

    // 1. Fundo principal
    FillRect(hdc, &rcClient, g_brBg);

    // 2. Top HeaderBar
    RECT rcHdr = { 0, oy, cw, oy + m.hdrH };
    FillRect(hdc, &rcHdr, g_brHeader);
    DrawHLine(hdc, 0, cw, oy + m.hdrH, RGB(54, 54, 54));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_TEXT_PRIMARY);
    SelectObject(hdc, g_hFontHdrTitle);
    RECT rcHdrT = { S(90), oy + S(5), cw - S(90), oy + S(25) };
    DrawTextA(hdc, T("AutoClicker Multi-Ponto", "AutoClicker Multi-Point"), -1, &rcHdrT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(hdc, g_show_saved_toast ? CLR_GREEN : CLR_TEXT_SUB);
    SelectObject(hdc, g_hFontHdrSub);
    RECT rcHdrS = { S(90), oy + S(24), cw - S(90), oy + S(42) };
    DrawTextA(hdc, g_show_saved_toast
              ? T("Configuracoes salvas com sucesso!", "Settings saved successfully!")
              : T("Suporte a multiplos monitores (X e Y)", "Multi-monitor support (X & Y)"),
              -1, &rcHdrS, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 3. Card Principal
    TargetConfig *t = CurTarget();
    int cx = m.pad;
    int cy = oy + m.cy;
    int cwCard = contentW;
    int chCard = m.chCard;
    if (g_scroll_y == 0 && (cy + chCard < ch - m.pad)) {
        chCard = (ch - m.pad) - cy;
    }

    FillRoundRect(hdc, cx, cy, cwCard, chCard, S(14), CLR_CARD, CLR_CARD_BORDER);

    int sx = cx + S(14);
    int sw = cwCard - S(28);

    // Titulo do Ponto
    const char *pTitle = (g_active_tab == 0)
        ? T("Ponto 1 (Tempo X)", "Point 1 (Time X)")
        : T("Ponto 2 (Tempo Y)", "Point 2 (Time Y)");
    SetTextColor(hdc, CLR_TEXT_PRIMARY);
    SelectObject(hdc, g_hFontCardTitle);
    RECT rcPTitle = { sx, cy + S(10), sx + sw - S(64), cy + S(34) };
    DrawTextA(hdc, pTitle, -1, &rcPTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Status & Cliques
    const char *stText = !t->enabled
        ? T("Desativado", "Disabled")
        : (g_is_running ? T("Ativo - Contando", "Active - Running") : T("Parado", "Stopped"));
    SetTextColor(hdc, CLR_TEXT_MUTED);
    SelectObject(hdc, g_hFontSmall);
    RECT rcSt = { sx, cy + S(36), sx + S(200), cy + S(52) };
    DrawTextA(hdc, stText, -1, &rcSt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    char cntBuf[48];
    if (g_lang == 1)
        wsprintfA(cntBuf, "%d click%s", t->total_clicks, (t->total_clicks == 1) ? "" : "s");
    else
        wsprintfA(cntBuf, "%d clique%s", t->total_clicks, (t->total_clicks == 1) ? "" : "s");
    SetTextColor(hdc, CLR_ACCENT);
    RECT rcCnt = { sx + sw - S(140), cy + S(36), sx + sw, cy + S(52) };
    DrawTextA(hdc, cntBuf, -1, &rcCnt, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // Cronometro Gigante Centralizado
    char timeBuf[32];
    FormatCountdown(t, timeBuf);
    SetTextColor(hdc, t->enabled ? CLR_TEXT_PRIMARY : CLR_TEXT_SUB);
    SelectObject(hdc, g_hFontTimer);
    RECT rcTimer = { sx, cy + S(48), sx + sw, cy + S(86) };
    DrawTextA(hdc, timeBuf, -1, &rcTimer, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Barra de Progresso Vermelha
    int by = cy + S(92), bh = S(6);
    FillRoundRect(hdc, sx, by, sw, bh, bh, CLR_TRACK_BG, CLR_TRACK_BG);
    if (t->enabled && t->interval_ms > 0) {
        DWORD rem_ms = t->interval_ms;
        if (g_is_running) {
            DWORD now = GetTickCount();
            rem_ms = ((LONG)(t->next_tick - now) > 0) ? (t->next_tick - now) : 0;
        }
        int fillW = (int)(((LONGLONG)rem_ms * sw) / t->interval_ms);
        if (fillW > sw) fillW = sw;
        if (fillW > S(4)) {
            FillRoundRect(hdc, sx, by, fillW, bh, bh, CLR_ACCENT, CLR_ACCENT);
        }
    }

    // SECAO 1: Click Location
    int g1y = oy + m.g1y;
    SetTextColor(hdc, CLR_TEXT_PRIMARY);
    SelectObject(hdc, g_hFontSection);
    RECT rcS1 = { sx, g1y - S(22), sx + sw, g1y - S(3) };
    DrawTextA(hdc, T("Local do Clique (Multi-Monitor)", "Click Location (Multi-Monitor)"), -1, &rcS1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int g1h = m.rH1 * 4;
    FillRoundRect(hdc, sx, g1y, sw, g1h, S(12), CLR_ROW, CLR_DIVIDER);
    DrawHLine(hdc, sx + 1, sx + sw - 1, g1y + m.rH1,     CLR_DIVIDER);
    DrawHLine(hdc, sx + 1, sx + sw - 1, g1y + m.rH1 * 2, CLR_DIVIDER);
    DrawHLine(hdc, sx + 1, sx + sw - 1, g1y + m.rH1 * 3, CLR_DIVIDER);

    // Row 1
    SetTextColor(hdc, CLR_TEXT_PRIMARY); SelectObject(hdc, g_hFontUi);
    RECT r1t = { sx + S(12), g1y + S(4), sx + sw - S(68), g1y + S(21) };
    DrawTextA(hdc, T("Usar Posicao Fixa na Tela", "Use Fixed Screen Position"), -1, &r1t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(hdc, CLR_TEXT_SUB); SelectObject(hdc, g_hFontSmall);
    RECT r1s = { sx + S(12), g1y + S(21), sx + sw - S(68), g1y + S(36) };
    DrawTextA(hdc, T("Mover o mouse para coordenadas especificas antes de clicar", "Move mouse to specific coordinates before clicking"),
              -1, &r1s, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Row 2
    SetTextColor(hdc, CLR_TEXT_PRIMARY); SelectObject(hdc, g_hFontUi);
    RECT r2t = { sx + S(12), g1y + m.rH1 + S(4), sx + sw - S(166), g1y + m.rH1 + S(21) };
    DrawTextA(hdc, T("Selecionar Alvo Visualmente", "Pick Target Visually"), -1, &r2t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(hdc, CLR_TEXT_SUB); SelectObject(hdc, g_hFontSmall);
    RECT r2s = { sx + S(12), g1y + m.rH1 + S(21), sx + sw - S(166), g1y + m.rH1 + S(36) };
    char coordHint[80];
    wsprintfA(coordHint, T("Alvo atual: Global (%d, %d)", "Current target: Global (%d, %d)"), t->x, t->y);
    DrawTextA(hdc, coordHint, -1, &r2s, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Row 3 & Row 4
    SetTextColor(hdc, CLR_TEXT_PRIMARY); SelectObject(hdc, g_hFontUi);
    RECT r3t = { sx + S(12), g1y + m.rH1 * 2, sx + sw - S(150), g1y + m.rH1 * 3 };
    DrawTextA(hdc, T("Coordenada Global X (pixels)", "Global X Coordinate (pixels)"), -1, &r3t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT r4t = { sx + S(12), g1y + m.rH1 * 3, sx + sw - S(150), g1y + m.rH1 * 4 };
    DrawTextA(hdc, T("Coordenada Global Y (pixels)", "Global Y Coordinate (pixels)"), -1, &r4t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // SECAO 2: Time Interval
    int g2y = oy + m.g2y;
    SetTextColor(hdc, CLR_TEXT_PRIMARY);
    SelectObject(hdc, g_hFontSection);
    RECT rcS2 = { sx, g2y - S(22), sx + sw, g2y - S(3) };
    DrawTextA(hdc, T("Intervalo de Tempo", "Time Interval"), -1, &rcS2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int g2h = m.rH2 * 2;
    FillRoundRect(hdc, sx, g2y, sw, g2h, S(12), CLR_ROW, CLR_DIVIDER);
    DrawHLine(hdc, sx + 1, sx + sw - 1, g2y + m.rH2, CLR_DIVIDER);

    SetTextColor(hdc, CLR_TEXT_PRIMARY); SelectObject(hdc, g_hFontUi);
    RECT r5t = { sx + S(12), g2y, sx + sw - S(150), g2y + m.rH2 };
    DrawTextA(hdc, T("Minutos", "Minutes"), -1, &r5t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT r6t = { sx + S(12), g2y + m.rH2, sx + sw - S(150), g2y + m.rH2 * 2 };
    DrawTextA(hdc, T("Segundos adicionais", "Additional seconds"), -1, &r6t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // SECAO 3: Button Options
    int g3y = oy + m.g3y;
    SetTextColor(hdc, CLR_TEXT_PRIMARY);
    SelectObject(hdc, g_hFontSection);
    RECT rcS3 = { sx, g3y - S(22), sx + sw, g3y - S(3) };
    DrawTextA(hdc, T("Opcoes do Botao", "Button Options"), -1, &rcS3, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int g3h = m.rH3 * 2;
    FillRoundRect(hdc, sx, g3y, sw, g3h, S(12), CLR_ROW, CLR_DIVIDER);
    DrawHLine(hdc, sx + 1, sx + sw - 1, g3y + m.rH3, CLR_DIVIDER);

    SetTextColor(hdc, CLR_TEXT_PRIMARY); SelectObject(hdc, g_hFontUi);
    RECT r7t = { sx + S(12), g3y, sx + sw - S(140), g3y + m.rH3 };
    DrawTextA(hdc, T("Botao do Mouse", "Mouse Button"), -1, &r7t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT r8t = { sx + S(12), g3y + m.rH3, sx + sw - S(140), g3y + m.rH3 * 2 };
    DrawTextA(hdc, T("Clique Duplo", "Double Click"), -1, &r8t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    BitBlt(hdcScreen, 0, 0, cw, ch, hdc, 0, 0, SRCCOPY);

    SelectObject(hdc, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdc);
    EndPaint(hwnd, &ps);
}

static void DrawOwnerButton(DRAWITEMSTRUCT *dis) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int id = (int)dis->CtlID;
    BOOL pressed = (dis->itemState & ODS_SELECTED) != 0;

    HBRUSH bgBr = g_brCard;
    if (id == IDC_BTN_SAVE || id == IDC_BTN_LANG) bgBr = g_brHeader;
    else if (id == IDC_BTN_TOGGLE || id == IDC_TAB_P1 || id == IDC_TAB_P2) bgBr = g_brBg;
    else if (id == IDC_SW_FIXED || id == IDC_BTN_PICK || id == IDC_BTN_MBTN || id == IDC_SW_DBL ||
             (id >= IDC_BTN_X_DEC && id <= IDC_BTN_SEC_INC)) bgBr = g_brRow;

    FillRect(hdc, &rc, bgBr);
    SetBkMode(hdc, TRANSPARENT);

    if (id == IDC_BTN_SAVE) {
        if (pressed) FillRoundRect(hdc, 0, 0, w, h, S(8), CLR_BTN_DARK, CLR_BTN_DARK);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        SelectObject(hdc, g_hFontUiBold);
        DrawTextA(hdc, T("Salvar", "Save"), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    if (id == IDC_BTN_LANG) {
        COLORREF bg = pressed ? CLR_BTN_DARK_PRS : CLR_BTN_DARK;
        FillRoundRect(hdc, 0, S(2), w, h - S(4), S(8), bg, CLR_DIVIDER);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        SelectObject(hdc, g_hFontSmall);
        DrawTextA(hdc, (g_lang == 1) ? "EN" : "PT", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    if (id == IDC_BTN_TOGGLE) {
        COLORREF bg = g_is_running ? (pressed ? CLR_ACCENT_PRESS : CLR_STOP_DARK)
                                   : (pressed ? CLR_ACCENT_PRESS : CLR_ACCENT);
        FillRoundRect(hdc, 0, 0, w, h, h / 2, bg, bg);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        SelectObject(hdc, g_hFontBtnBig);
        DrawTextA(hdc, g_is_running
                  ? T("Parar AutoClicker (Ambos os Pontos)", "Stop AutoClicker (Both Points)")
                  : T("Iniciar AutoClicker (Ambos os Pontos)", "Start AutoClicker (Both Points)"),
                  -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    if (id == IDC_TAB_P1 || id == IDC_TAB_P2) {
        int idx = (id == IDC_TAB_P1) ? 0 : 1;
        BOOL active = (g_active_tab == idx);
        COLORREF bg = active ? CLR_TAB_ACTIVE : CLR_TAB_BG;
        COLORREF bd = active ? CLR_ACCENT : CLR_CARD_BORDER;
        FillRoundRect(hdc, 0, 0, w, h, S(10), bg, bd);

        int dotSz = S(8);
        COLORREF dot = g_targets[idx].enabled ? CLR_ACCENT : CLR_SWITCH_OFF;
        FillRoundRect(hdc, S(12), (h - dotSz) / 2, dotSz, dotSz, dotSz, dot, dot);

        char tBuf[16], lbl[64];
        FormatCountdown(&g_targets[idx], tBuf);
        if (g_lang == 1)
            wsprintfA(lbl, "Point %d (%s)  -  %s", idx + 1, (idx == 0) ? "Time X" : "Time Y", tBuf);
        else
            wsprintfA(lbl, "Ponto %d (%s)  -  %s", idx + 1, (idx == 0) ? "Tempo X" : "Tempo Y", tBuf);
        SetTextColor(hdc, active ? CLR_TEXT_PRIMARY : CLR_TEXT_MUTED);
        SelectObject(hdc, active ? g_hFontUiBold : g_hFontUi);
        RECT rcT = { S(24), 0, w - S(8), h };
        DrawTextA(hdc, lbl, -1, &rcT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    if (id == IDC_SW_ENABLED || id == IDC_SW_FIXED || id == IDC_SW_DBL) {
        TargetConfig *t = CurTarget();
        BOOL isOn = (id == IDC_SW_ENABLED) ? t->enabled :
                    (id == IDC_SW_FIXED)   ? t->use_fixed : t->double_click;

        int swW = S(42), swH = S(22);
        int swX = (w - swW) / 2;
        int swY = (h - swH) / 2;
        COLORREF track = isOn ? CLR_ACCENT : CLR_SWITCH_OFF;
        FillRoundRect(hdc, swX, swY, swW, swH, swH, track, track);

        int padK = S(3);
        int knob = swH - padK * 2;
        int kx = isOn ? (swX + swW - knob - padK) : (swX + padK);
        int ky = swY + padK;
        FillRoundRect(hdc, kx, ky, knob, knob, knob, CLR_KNOB, CLR_KNOB);
        return;
    }

    if (id == IDC_BTN_PICK) {
        COLORREF bg = pressed ? CLR_ACCENT_PRESS : CLR_ACCENT;
        FillRoundRect(hdc, 0, 0, w, h, S(8), bg, bg);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        SelectObject(hdc, g_hFontUiBold);
        DrawTextA(hdc, (g_picking_target >= 0)
                  ? T("Clique no alvo...", "Click target...")
                  : T("Marcar na Tela", "Pick on Screen"),
                  -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    if (id == IDC_BTN_X_DEC || id == IDC_BTN_X_INC ||
        id == IDC_BTN_Y_DEC || id == IDC_BTN_Y_INC ||
        id == IDC_BTN_MIN_DEC || id == IDC_BTN_MIN_INC ||
        id == IDC_BTN_SEC_DEC || id == IDC_BTN_SEC_INC) {
        COLORREF bg = pressed ? CLR_BTN_DARK_PRS : CLR_BTN_DARK;
        FillRoundRect(hdc, 1, 1, w - 2, h - 2, w - 2, bg, bg);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        SelectObject(hdc, g_hFontUiBold);
        BOOL isInc = (id == IDC_BTN_X_INC || id == IDC_BTN_Y_INC || id == IDC_BTN_MIN_INC || id == IDC_BTN_SEC_INC);
        DrawTextA(hdc, isInc ? "+" : "-", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    if (id >= IDC_PRESET_5S && id <= IDC_PRESET_10M) {
        const char *labels[7] = { "5s", "10s", "30s", "1 min", "2 min", "5 min", "10 min" };
        if (pressed) FillRoundRect(hdc, 0, 0, w, h, S(8), CLR_BTN_DARK, CLR_BTN_DARK);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        SelectObject(hdc, g_hFontUiBold);
        DrawTextA(hdc, labels[id - IDC_PRESET_5S], -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    if (id == IDC_BTN_MBTN) {
        if (pressed) FillRoundRect(hdc, 0, 0, w, h, S(6), CLR_BTN_DARK, CLR_BTN_DARK);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        SelectObject(hdc, g_hFontUi);
        DrawTextA(hdc, GetMouseBtnLabel(CurTarget()->button), -1, &rc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    if (id == IDC_BTN_TEST) {
        COLORREF bg = pressed ? CLR_BTN_DARK_PRS : CLR_BTN_DARK;
        FillRoundRect(hdc, 0, 0, w, h, S(10), bg, bg);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        SelectObject(hdc, g_hFontUiBold);
        const char *txt = (g_active_tab == 0)
            ? T("Testar clique no Ponto 1 (Tempo X) agora", "Test click on Point 1 (Time X) now")
            : T("Testar clique no Ponto 2 (Tempo Y) agora", "Test click on Point 2 (Time Y) now");
        DrawTextA(hdc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
}

static void ApplyPreset(int mins, int secs) {
    TargetConfig *t = CurTarget();
    t->minutes = mins;
    t->seconds = secs;
    RecalcInterval(t);
    UpdateEditBoxesFromTarget();
    SaveConfig();
    RedrawAll();
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HDC hdc = GetDC(hwnd);
            if (hdc) {
                int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
                if (dpi >= 96) g_dpi = dpi;
                ReleaseDC(hwnd, hdc);
            }
            CreateScaledFonts();

            g_brBg     = CreateSolidBrush(CLR_BG);
            g_brHeader = CreateSolidBrush(CLR_HEADER);
            g_brCard   = CreateSolidBrush(CLR_CARD);
            g_brRow    = CreateSolidBrush(CLR_ROW);

            BOOL useDark = TRUE;
            DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
            DWORD cornerPref = 2; // DWMWCP_ROUND
            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));
            break;
        }

        case WM_DPICHANGED: {
            int newDpi = HIWORD(wParam);
            if (newDpi >= 96) {
                g_dpi = newDpi;
                CreateScaledFonts();
                ApplyFontsToEdits(hwnd);
                RECT *prc = (RECT *)lParam;
                SetWindowPos(hwnd, NULL, prc->left, prc->top,
                             prc->right - prc->left, prc->bottom - prc->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
                LayoutControls(hwnd);
                RedrawAll();
            }
            return 0;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO *)lParam;
            RECT rc = { 0, 0, S(MIN_CLIENT_W), S(MIN_CLIENT_H) };
            AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
            mmi->ptMinTrackSize.x = rc.right - rc.left;
            mmi->ptMinTrackSize.y = rc.bottom - rc.top;
            return 0;
        }

        case WM_SIZE:
            LayoutControls(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int step = (delta / 120) * S(48);
            if (step == 0) step = (delta > 0) ? S(48) : -S(48);
            SetScrollPosAndRefresh(hwnd, g_scroll_y - step);
            return 0;
        }

        case WM_VSCROLL: {
            SCROLLINFO si = {0};
            si.cbSize = sizeof(SCROLLINFO);
            si.fMask  = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);
            int pos = g_scroll_y;
            switch (LOWORD(wParam)) {
                case SB_LINEUP:        pos -= S(32); break;
                case SB_LINEDOWN:      pos += S(32); break;
                case SB_PAGEUP:        pos -= (int)si.nPage; break;
                case SB_PAGEDOWN:      pos += (int)si.nPage; break;
                case SB_THUMBTRACK:
                case SB_THUMBPOSITION: pos = si.nTrackPos; break;
            }
            SetScrollPosAndRefresh(hwnd, pos);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
            PaintMainWindow(hwnd);
            return 0;

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, CLR_TEXT_PRIMARY);
            SetBkColor(hdcEdit, CLR_ROW);
            return (LRESULT)g_brRow;
        }

        case WM_DRAWITEM:
            DrawOwnerButton((DRAWITEMSTRUCT *)lParam);
            return TRUE;

        case WM_LBUTTONDOWN:
            if (g_picking_target >= 0) {
                POINT pt;
                GetCursorPos(&pt);
                FinishCapture(pt.x, pt.y);
                return 0;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE && g_picking_target >= 0) {
                CancelCapture();
                return 0;
            }
            if (wParam == VK_F6 || wParam == VK_SPACE) {
                ToggleRunning();
                return 0;
            }
            break;

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);

            if (code == EN_CHANGE && (id == IDC_EDT_X || id == IDC_EDT_Y || id == IDC_EDT_MIN || id == IDC_EDT_SEC)) {
                SyncEditBoxesToTarget();
                if (!g_is_running) {
                    InvalidateRect(hwnd, NULL, FALSE);
                    InvalidateRect(GetDlgItem(hwnd, IDC_TAB_P1), NULL, FALSE);
                    InvalidateRect(GetDlgItem(hwnd, IDC_TAB_P2), NULL, FALSE);
                }
                break;
            }

            if (code != BN_CLICKED) break;

            TargetConfig *t = CurTarget();

            if (id == IDC_BTN_TOGGLE) {
                ToggleRunning();
            } else if (id == IDC_BTN_SAVE) {
                SyncEditBoxesToTarget();
                SaveConfig();
                g_show_saved_toast = TRUE;
                SetTimer(hwnd, TIMER_TOAST, 2500, NULL);
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (id == IDC_BTN_LANG) {
                g_lang = (g_lang == 1) ? 0 : 1;
                SaveConfig();
                RedrawAll();
            } else if (id == IDC_TAB_P1 && g_active_tab != 0) {
                SyncEditBoxesToTarget();
                g_active_tab = 0;
                UpdateEditBoxesFromTarget();
                RedrawAll();
            } else if (id == IDC_TAB_P2 && g_active_tab != 1) {
                SyncEditBoxesToTarget();
                g_active_tab = 1;
                UpdateEditBoxesFromTarget();
                RedrawAll();
            } else if (id == IDC_SW_ENABLED) {
                t->enabled = !t->enabled;
                SaveConfig();
                RedrawAll();
            } else if (id == IDC_SW_FIXED) {
                t->use_fixed = !t->use_fixed;
                SaveConfig();
                RedrawAll();
            } else if (id == IDC_SW_DBL) {
                t->double_click = !t->double_click;
                SaveConfig();
                InvalidateRect(GetDlgItem(hwnd, id), NULL, FALSE);
            } else if (id == IDC_BTN_PICK) {
                StartCapture(g_active_tab);
            } else if (id == IDC_BTN_MBTN) {
                t->button = (t->button + 1) % 3;
                SaveConfig();
                InvalidateRect(GetDlgItem(hwnd, id), NULL, FALSE);
            } else if (id == IDC_BTN_TEST) {
                SyncEditBoxesToTarget();
                ExecuteClick(t);
                RedrawAll();
            } else if (id == IDC_BTN_X_DEC) { t->x -= 10; UpdateEditBoxesFromTarget(); SaveConfig(); RedrawAll(); }
              else if (id == IDC_BTN_X_INC) { t->x += 10; UpdateEditBoxesFromTarget(); SaveConfig(); RedrawAll(); }
              else if (id == IDC_BTN_Y_DEC) { t->y -= 10; UpdateEditBoxesFromTarget(); SaveConfig(); RedrawAll(); }
              else if (id == IDC_BTN_Y_INC) { t->y += 10; UpdateEditBoxesFromTarget(); SaveConfig(); RedrawAll(); }
              else if (id == IDC_BTN_MIN_DEC) { t->minutes--; RecalcInterval(t); UpdateEditBoxesFromTarget(); SaveConfig(); RedrawAll(); }
              else if (id == IDC_BTN_MIN_INC) { t->minutes++; RecalcInterval(t); UpdateEditBoxesFromTarget(); SaveConfig(); RedrawAll(); }
              else if (id == IDC_BTN_SEC_DEC) { t->seconds--; RecalcInterval(t); UpdateEditBoxesFromTarget(); SaveConfig(); RedrawAll(); }
              else if (id == IDC_BTN_SEC_INC) { t->seconds++; RecalcInterval(t); UpdateEditBoxesFromTarget(); SaveConfig(); RedrawAll(); }
              else if (id == IDC_PRESET_5S)  { ApplyPreset(0, 5); }
              else if (id == IDC_PRESET_10S) { ApplyPreset(0, 10); }
              else if (id == IDC_PRESET_30S) { ApplyPreset(0, 30); }
              else if (id == IDC_PRESET_1M)  { ApplyPreset(1, 0); }
              else if (id == IDC_PRESET_2M)  { ApplyPreset(2, 0); }
              else if (id == IDC_PRESET_5M)  { ApplyPreset(5, 0); }
              else if (id == IDC_PRESET_10M) { ApplyPreset(10, 0); }
            break;
        }

        case WM_TIMER: {
            if (wParam == TIMER_TOAST) {
                KillTimer(hwnd, TIMER_TOAST);
                g_show_saved_toast = FALSE;
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (wParam == TIMER_TICK && g_is_running) {
                DWORD now = GetTickCount();
                for (int i = 0; i < 2; i++) {
                    TargetConfig *t = &g_targets[i];
                    if (t->enabled && (LONG)(now - t->next_tick) >= 0) {
                        ExecuteClick(t);
                        t->next_tick = GetTickCount() + t->interval_ms;
                    }
                }
                InvalidateRect(hwnd, NULL, FALSE);
                InvalidateRect(GetDlgItem(hwnd, IDC_TAB_P1), NULL, FALSE);
                InvalidateRect(GetDlgItem(hwnd, IDC_TAB_P2), NULL, FALSE);
            }
            break;
        }

        case WM_CLOSE:
            SyncEditBoxesToTarget();
            SaveConfig();
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            DestroyFonts();
            if (g_brBg) DeleteObject(g_brBg);
            if (g_brHeader) DeleteObject(g_brHeader);
            if (g_brCard) DeleteObject(g_brCard);
            if (g_brRow) DeleteObject(g_brRow);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static void CreateDarkEdit(HWND hParent, HINSTANCE hInst, int id, BOOL numericOnly) {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_RIGHT | ES_AUTOHSCROLL;
    if (numericOnly) style |= ES_NUMBER;
    HWND hEdit = CreateWindowExA(0, "EDIT", "", style, 0, 0, 10, 10, hParent, (HMENU)(INT_PTR)id, hInst, NULL);
    SendMessageA(hEdit, WM_SETFONT, (WPARAM)g_hFontUiBold, TRUE);
}

static void CreateOwnerBtn(HWND hParent, HINSTANCE hInst, int id) {
    CreateWindowExA(0, "BUTTON", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 10, 10, hParent, (HMENU)(INT_PTR)id, hInst, NULL);
}

static void CreateMainControls(HWND hWnd, HINSTANCE hInst) {
    CreateOwnerBtn(hWnd, hInst, IDC_BTN_SAVE);
    CreateOwnerBtn(hWnd, hInst, IDC_BTN_LANG);
    CreateOwnerBtn(hWnd, hInst, IDC_BTN_TOGGLE);
    CreateOwnerBtn(hWnd, hInst, IDC_TAB_P1);
    CreateOwnerBtn(hWnd, hInst, IDC_TAB_P2);

    CreateOwnerBtn(hWnd, hInst, IDC_SW_ENABLED);
    CreateOwnerBtn(hWnd, hInst, IDC_SW_FIXED);
    CreateOwnerBtn(hWnd, hInst, IDC_BTN_PICK);

    CreateDarkEdit(hWnd, hInst, IDC_EDT_X, FALSE);
    CreateOwnerBtn(hWnd, hInst, IDC_BTN_X_DEC);
    CreateOwnerBtn(hWnd, hInst, IDC_BTN_X_INC);

    CreateDarkEdit(hWnd, hInst, IDC_EDT_Y, FALSE);
    CreateOwnerBtn(hWnd, hInst, IDC_BTN_Y_DEC);
    CreateOwnerBtn(hWnd, hInst, IDC_BTN_Y_INC);

    CreateDarkEdit(hWnd, hInst, IDC_EDT_MIN, TRUE);
    CreateOwnerBtn(hWnd, hInst, IDC_BTN_MIN_DEC);
    CreateOwnerBtn(hWnd, hInst, IDC_BTN_MIN_INC);

    CreateDarkEdit(hWnd, hInst, IDC_EDT_SEC, TRUE);
    CreateOwnerBtn(hWnd, hInst, IDC_BTN_SEC_DEC);
    CreateOwnerBtn(hWnd, hInst, IDC_BTN_SEC_INC);

    for (int i = 0; i < 7; i++) {
        CreateOwnerBtn(hWnd, hInst, IDC_PRESET_5S + i);
    }

    CreateOwnerBtn(hWnd, hInst, IDC_BTN_MBTN);
    CreateOwnerBtn(hWnd, hInst, IDC_SW_DBL);
    CreateOwnerBtn(hWnd, hInst, IDC_BTN_TEST);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInst;
    (void)lpCmdLine;

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    HDC hdcScr = GetDC(NULL);
    if (hdcScr) {
        int dpi = GetDeviceCaps(hdcScr, LOGPIXELSX);
        if (dpi >= 96) g_dpi = dpi;
        ReleaseDC(NULL, hdcScr);
    }

    LoadConfig();

    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = "AutoClickerAdwaitaDarkClass";
    wc.hbrBackground = NULL;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.hIcon         = LoadIconA(hInst, MAKEINTRESOURCEA(IDI_APP_ICON));
    if (!wc.hIcon) {
        wc.hIcon = LoadIconA(NULL, IDI_APPLICATION);
    }

    if (!RegisterClassA(&wc)) {
        MessageBoxA(NULL, "Failed to register window class.", "AutoClicker", MB_ICONERROR);
        return 1;
    }

    DWORD style = WS_OVERLAPPEDWINDOW | WS_VSCROLL;
    RECT rcWin = { 0, 0, S(BASE_CLIENT_W), S(BASE_CLIENT_H) };
    AdjustWindowRect(&rcWin, style, FALSE);

    int winW = rcWin.right - rcWin.left;
    int winH = rcWin.bottom - rcWin.top;
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    if (winH > scrH - 60) winH = scrH - 60;
    int posX = (scrW - winW) / 2;
    int posY = (scrH - winH) / 2;
    if (posX < 0) posX = 40;
    if (posY < 0) posY = 20;

    g_hMainWnd = CreateWindowExA(
        0, "AutoClickerAdwaitaDarkClass", T("AutoClicker Multi-Ponto", "AutoClicker Multi-Point"),
        style,
        posX, posY, winW, winH,
        NULL, NULL, hInst, NULL
    );

    if (!g_hMainWnd) {
        MessageBoxA(NULL, "Failed to create main window.", "AutoClicker", MB_ICONERROR);
        return 1;
    }

    CreateMainControls(g_hMainWnd, hInst);
    LayoutControls(g_hMainWnd);
    UpdateEditBoxesFromTarget();

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return (int)msg.wParam;
}

void WinMainCRTStartup(void) {
    HINSTANCE hInst = GetModuleHandleA(NULL);
    STARTUPINFOA si;
    si.cb = sizeof(STARTUPINFOA);
    GetStartupInfoA(&si);

    int nCmdShow = (si.dwFlags & STARTF_USESHOWWINDOW) ? si.wShowWindow : SW_SHOWDEFAULT;
    int ret = WinMain(hInst, NULL, GetCommandLineA(), nCmdShow);
    ExitProcess((UINT)ret);
}
