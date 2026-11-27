/*
 * AutoClicker Multi-Ponto para Windows (Adwaita Dark Red UI)
 * Design fiel a versao Linux GTK4/Libadwaita com tema escuro nativo,
 * acento vermelho vibrante (#E0344A), cards agrupados, steppers (- / +),
 * presets rapidos (5s a 10 min), seletor visual multi-monitor e persistencia.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>

#define IDI_APP_ICON 101
#define TIMER_TICK   1001
#define TIMER_TOAST  1002

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

#define WIN_CLIENT_W      540
#define WIN_CLIENT_H      754

// IDs de controles
#define IDC_BTN_SAVE         2001
#define IDC_BTN_TOGGLE       2002
#define IDC_TAB_P1           2003
#define IDC_TAB_P2           2004

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
    int button; // 0=Esquerdo, 1=Direito, 2=Meio
    BOOL double_click;
    DWORD next_tick;
    DWORD interval_ms;
    int total_clicks;
} TargetConfig;

static TargetConfig g_targets[2] = {
    { TRUE, TRUE, 500, 400, 2, 0, 0, FALSE, 0, 120000, 0 },
    { TRUE, TRUE, 800, 600, 5, 0, 0, FALSE, 0, 300000, 0 }
};

static int g_active_tab = 0; // 0 = Ponto 1, 1 = Ponto 2
static BOOL g_is_running = FALSE;
static BOOL g_play_sound = TRUE;
static BOOL g_show_saved_toast = FALSE;
static BOOL g_updating_ui = FALSE;
static int g_picking_target = -1; // -1 = nenhum, 0 = P1, 1 = P2

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
    if (btn == 1) return "Direito  v";
    if (btn == 2) return "Meio  v";
    return "Esquerdo  v";
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
    InvalidateRect(g_hMainWnd, NULL, FALSE);
    // Forcar redesenho dos botoes owner-draw
    int ids[] = {
        IDC_BTN_SAVE, IDC_BTN_TOGGLE, IDC_TAB_P1, IDC_TAB_P2,
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

    HDC hdc = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, cw, ch);
    HBITMAP oldBmp = (HBITMAP)SelectObject(hdc, hBmp);

    // 1. Fundo principal
    FillRect(hdc, &rcClient, g_brBg);

    // 2. Top HeaderBar (#2A2A2A)
    RECT rcHdr = { 0, 0, cw, 46 };
    FillRect(hdc, &rcHdr, g_brHeader);
    DrawHLine(hdc, 0, cw, 46, RGB(54, 54, 54));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_TEXT_PRIMARY);
    SelectObject(hdc, g_hFontHdrTitle);
    RECT rcHdrT = { 90, 6, cw - 90, 26 };
    DrawTextA(hdc, "AutoClicker Multi-Ponto", -1, &rcHdrT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(hdc, g_show_saved_toast ? CLR_GREEN : CLR_TEXT_SUB);
    SelectObject(hdc, g_hFontHdrSub);
    RECT rcHdrS = { 90, 25, cw - 90, 42 };
    DrawTextA(hdc, g_show_saved_toast ? "Configuracoes salvas com sucesso!" : "Suporte a multiplos monitores (X e Y)",
              -1, &rcHdrS, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 3. Card Principal (#2E2E2E)
    TargetConfig *t = CurTarget();
    int cx = 18, cy = 148, cwCard = 504, chCard = 592;
    FillRoundRect(hdc, cx, cy, cwCard, chCard, 14, CLR_CARD, CLR_CARD_BORDER);

    // Titulo do Ponto dentro do Card
    const char *pTitle = (g_active_tab == 0) ? "Ponto 1 (Tempo X)" : "Ponto 2 (Tempo Y)";
    SetTextColor(hdc, CLR_TEXT_PRIMARY);
    SelectObject(hdc, g_hFontCardTitle);
    RECT rcPTitle = { cx + 16, cy + 12, cx + 320, cy + 36 };
    DrawTextA(hdc, pTitle, -1, &rcPTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Status (esq) e Cliques (dir)
    const char *stText = !t->enabled ? "Desativado" : (g_is_running ? "Ativo - Contando" : "Parado");
    SetTextColor(hdc, CLR_TEXT_MUTED);
    SelectObject(hdc, g_hFontSmall);
    RECT rcSt = { cx + 16, cy + 38, cx + 220, cy + 54 };
    DrawTextA(hdc, stText, -1, &rcSt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    char cntBuf[48];
    wsprintfA(cntBuf, "%d clique%s", t->total_clicks, (t->total_clicks == 1) ? "" : "s");
    SetTextColor(hdc, CLR_ACCENT);
    RECT rcCnt = { cx + cwCard - 140, cy + 38, cx + cwCard - 16, cy + 54 };
    DrawTextA(hdc, cntBuf, -1, &rcCnt, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // Cronometro Gigante Centralizado (02:00)
    char timeBuf[32];
    FormatCountdown(t, timeBuf);
    SetTextColor(hdc, t->enabled ? CLR_TEXT_PRIMARY : CLR_TEXT_SUB);
    SelectObject(hdc, g_hFontTimer);
    RECT rcTimer = { cx + 16, cy + 52, cx + cwCard - 16, cy + 90 };
    DrawTextA(hdc, timeBuf, -1, &rcTimer, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Barra de Progresso Vermelha arredondada
    int bx = cx + 16, by = cy + 96, bw = cwCard - 32, bh = 6;
    FillRoundRect(hdc, bx, by, bw, bh, 6, CLR_TRACK_BG, CLR_TRACK_BG);
    if (t->enabled && t->interval_ms > 0) {
        DWORD rem_ms = t->interval_ms;
        if (g_is_running) {
            DWORD now = GetTickCount();
            rem_ms = ((LONG)(t->next_tick - now) > 0) ? (t->next_tick - now) : 0;
        }
        int fillW = (int)(((LONGLONG)rem_ms * bw) / t->interval_ms);
        if (fillW > bw) fillW = bw;
        if (fillW > 4) {
            FillRoundRect(hdc, bx, by, fillW, bh, 6, CLR_ACCENT, CLR_ACCENT);
        }
    }

    // ==============================================
    // SECAO 1: Local do Clique (Multi-Monitor)
    // ==============================================
    int sx = cx + 16, sw = cwCard - 32;
    SetTextColor(hdc, CLR_TEXT_PRIMARY);
    SelectObject(hdc, g_hFontSection);
    RECT rcS1 = { sx, cy + 114, sx + sw, cy + 132 };
    DrawTextA(hdc, "Local do Clique (Multi-Monitor)", -1, &rcS1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int g1y = cy + 136, g1h = 168; // 4 linhas x 42px
    FillRoundRect(hdc, sx, g1y, sw, g1h, 12, CLR_ROW, CLR_DIVIDER);
    DrawHLine(hdc, sx + 1, sx + sw - 1, g1y + 42, CLR_DIVIDER);
    DrawHLine(hdc, sx + 1, sx + sw - 1, g1y + 84, CLR_DIVIDER);
    DrawHLine(hdc, sx + 1, sx + sw - 1, g1y + 126, CLR_DIVIDER);

    // Row 1: Usar Posicao Fixa
    SetTextColor(hdc, CLR_TEXT_PRIMARY); SelectObject(hdc, g_hFontUi);
    RECT r1t = { sx + 14, g1y + 6, sx + 360, g1y + 23 };
    DrawTextA(hdc, "Usar Posicao Fixa na Tela", -1, &r1t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(hdc, CLR_TEXT_SUB); SelectObject(hdc, g_hFontSmall);
    RECT r1s = { sx + 14, g1y + 22, sx + 380, g1y + 38 };
    DrawTextA(hdc, "Mover o mouse para coordenadas especificas antes de clicar", -1, &r1s, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Row 2: Selecionar Alvo Visualmente
    SetTextColor(hdc, CLR_TEXT_PRIMARY); SelectObject(hdc, g_hFontUi);
    RECT r2t = { sx + 14, g1y + 48, sx + 320, g1y + 65 };
    DrawTextA(hdc, "Selecionar Alvo Visualmente", -1, &r2t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(hdc, CLR_TEXT_SUB); SelectObject(hdc, g_hFontSmall);
    RECT r2s = { sx + 14, g1y + 64, sx + 320, g1y + 80 };
    char coordHint[80];
    wsprintfA(coordHint, "Alvo atual: Global (%d, %d)", t->x, t->y);
    DrawTextA(hdc, coordHint, -1, &r2s, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Row 3: Coordenada Global X
    SetTextColor(hdc, CLR_TEXT_PRIMARY); SelectObject(hdc, g_hFontUi);
    RECT r3t = { sx + 14, g1y + 84, sx + 300, g1y + 126 };
    DrawTextA(hdc, "Coordenada Global X (pixels)", -1, &r3t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Row 4: Coordenada Global Y
    RECT r4t = { sx + 14, g1y + 126, sx + 300, g1y + 168 };
    DrawTextA(hdc, "Coordenada Global Y (pixels)", -1, &r4t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // ==============================================
    // SECAO 2: Intervalo de Tempo
    // ==============================================
    SetTextColor(hdc, CLR_TEXT_PRIMARY);
    SelectObject(hdc, g_hFontSection);
    RECT rcS2 = { sx, cy + 314, sx + sw, cy + 332 };
    DrawTextA(hdc, "Intervalo de Tempo", -1, &rcS2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int g2y = cy + 336, g2h = 76; // 2 linhas x 38px
    FillRoundRect(hdc, sx, g2y, sw, g2h, 12, CLR_ROW, CLR_DIVIDER);
    DrawHLine(hdc, sx + 1, sx + sw - 1, g2y + 38, CLR_DIVIDER);

    SetTextColor(hdc, CLR_TEXT_PRIMARY); SelectObject(hdc, g_hFontUi);
    RECT r5t = { sx + 14, g2y, sx + 300, g2y + 38 };
    DrawTextA(hdc, "Minutos", -1, &r5t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT r6t = { sx + 14, g2y + 38, sx + 300, g2y + 76 };
    DrawTextA(hdc, "Segundos adicionais", -1, &r6t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // ==============================================
    // SECAO 3: Opcoes do Botao
    // ==============================================
    SetTextColor(hdc, CLR_TEXT_PRIMARY);
    SelectObject(hdc, g_hFontSection);
    RECT rcS3 = { sx, cy + 454, sx + sw, cy + 472 };
    DrawTextA(hdc, "Opcoes do Botao", -1, &rcS3, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int g3y = cy + 476, g3h = 68; // 2 linhas x 34px
    FillRoundRect(hdc, sx, g3y, sw, g3h, 12, CLR_ROW, CLR_DIVIDER);
    DrawHLine(hdc, sx + 1, sx + sw - 1, g3y + 34, CLR_DIVIDER);

    SetTextColor(hdc, CLR_TEXT_PRIMARY); SelectObject(hdc, g_hFontUi);
    RECT r7t = { sx + 14, g3y, sx + 300, g3y + 34 };
    DrawTextA(hdc, "Botao do Mouse", -1, &r7t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT r8t = { sx + 14, g3y + 34, sx + 300, g3y + 68 };
    DrawTextA(hdc, "Clique Duplo", -1, &r8t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Blit sem flicker
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

    // Fundo atras dos cantos arredondados
    HBRUSH bgBr = g_brCard;
    if (id == IDC_BTN_SAVE) bgBr = g_brHeader;
    else if (id == IDC_BTN_TOGGLE || id == IDC_TAB_P1 || id == IDC_TAB_P2) bgBr = g_brBg;
    else if (id == IDC_SW_FIXED || id == IDC_BTN_PICK || id == IDC_BTN_MBTN || id == IDC_SW_DBL ||
             (id >= IDC_BTN_X_DEC && id <= IDC_BTN_SEC_INC)) bgBr = g_brRow;

    FillRect(hdc, &rc, bgBr);
    SetBkMode(hdc, TRANSPARENT);

    // 1. Botao Salvar no Header
    if (id == IDC_BTN_SAVE) {
        if (pressed) FillRoundRect(hdc, 0, 0, w, h, 8, CLR_BTN_DARK, CLR_BTN_DARK);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        SelectObject(hdc, g_hFontUiBold);
        DrawTextA(hdc, "Salvar", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    // 2. Botao Principal Iniciar / Parar (Pill Vermelho vibrante)
    if (id == IDC_BTN_TOGGLE) {
        COLORREF bg = g_is_running ? (pressed ? CLR_ACCENT_PRESS : CLR_STOP_DARK)
                                   : (pressed ? CLR_ACCENT_PRESS : CLR_ACCENT);
        FillRoundRect(hdc, 0, 0, w, h, 24, bg, bg);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        SelectObject(hdc, g_hFontBtnBig);
        DrawTextA(hdc, g_is_running ? "Parar AutoClicker (Ambos os Pontos)"
                                    : "Iniciar AutoClicker (Ambos os Pontos)",
                  -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    // 3. Abas Ponto 1 / Ponto 2
    if (id == IDC_TAB_P1 || id == IDC_TAB_P2) {
        int idx = (id == IDC_TAB_P1) ? 0 : 1;
        BOOL active = (g_active_tab == idx);
        COLORREF bg = active ? CLR_TAB_ACTIVE : CLR_TAB_BG;
        COLORREF bd = active ? CLR_ACCENT : CLR_CARD_BORDER;
        FillRoundRect(hdc, 0, 0, w, h, 10, bg, bd);

        // Bolinha indicadora de ativado
        COLORREF dot = g_targets[idx].enabled ? CLR_ACCENT : CLR_SWITCH_OFF;
        FillRoundRect(hdc, 12, (h - 8) / 2, 8, 8, 8, dot, dot);

        char tBuf[16], lbl[64];
        FormatCountdown(&g_targets[idx], tBuf);
        wsprintfA(lbl, "Ponto %d (%s)  -  %s", idx + 1, (idx == 0) ? "Tempo X" : "Tempo Y", tBuf);
        SetTextColor(hdc, active ? CLR_TEXT_PRIMARY : CLR_TEXT_MUTED);
        SelectObject(hdc, active ? g_hFontUiBold : g_hFontUi);
        RECT rcT = { 24, 0, w - 8, h };
        DrawTextA(hdc, lbl, -1, &rcT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    // 4. Toggle Switches (Adwaita Red Pill Switch)
    if (id == IDC_SW_ENABLED || id == IDC_SW_FIXED || id == IDC_SW_DBL) {
        TargetConfig *t = CurTarget();
        BOOL isOn = (id == IDC_SW_ENABLED) ? t->enabled :
                    (id == IDC_SW_FIXED)   ? t->use_fixed : t->double_click;

        int swW = 42, swH = 24;
        int swX = (w - swW) / 2;
        int swY = (h - swH) / 2;
        COLORREF track = isOn ? CLR_ACCENT : CLR_SWITCH_OFF;
        FillRoundRect(hdc, swX, swY, swW, swH, swH, track, track);

        int knob = 18;
        int kx = isOn ? (swX + swW - knob - 3) : (swX + 3);
        int ky = swY + 3;
        FillRoundRect(hdc, kx, ky, knob, knob, knob, CLR_KNOB, CLR_KNOB);
        return;
    }

    // 5. Botao "Marcar na Tela" (Vermelho vibrante igual ao print!)
    if (id == IDC_BTN_PICK) {
        COLORREF bg = pressed ? CLR_ACCENT_PRESS : CLR_ACCENT;
        FillRoundRect(hdc, 0, 0, w, h, 8, bg, bg);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        SelectObject(hdc, g_hFontUiBold);
        DrawTextA(hdc, (g_picking_target >= 0) ? "Clique no alvo..." : "Marcar na Tela",
                  -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    // 6. Botoes Steppers Circulares (-) e (+)
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

    // 7. Presets Rapidos (5s, 10s, 30s, 1 min, 2 min, 5 min, 10 min)
    if (id >= IDC_PRESET_5S && id <= IDC_PRESET_10M) {
        const char *labels[7] = { "5s", "10s", "30s", "1 min", "2 min", "5 min", "10 min" };
        if (pressed) FillRoundRect(hdc, 0, 0, w, h, 8, CLR_BTN_DARK, CLR_BTN_DARK);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        SelectObject(hdc, g_hFontUiBold);
        DrawTextA(hdc, labels[id - IDC_PRESET_5S], -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    // 8. Botao Selecao de Mouse (Esquerdo v)
    if (id == IDC_BTN_MBTN) {
        if (pressed) FillRoundRect(hdc, 0, 0, w, h, 6, CLR_BTN_DARK, CLR_BTN_DARK);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        SelectObject(hdc, g_hFontUi);
        DrawTextA(hdc, GetMouseBtnLabel(CurTarget()->button), -1, &rc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    // 9. Botao Testar Clique (Pill escuro na base do card)
    if (id == IDC_BTN_TEST) {
        COLORREF bg = pressed ? CLR_BTN_DARK_PRS : CLR_BTN_DARK;
        FillRoundRect(hdc, 0, 0, w, h, 10, bg, bg);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        SelectObject(hdc, g_hFontUiBold);
        const char *txt = (g_active_tab == 0) ? "Testar clique no Ponto 1 (Tempo X) agora"
                                              : "Testar clique no Ponto 2 (Tempo Y) agora";
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
            g_hFontHdrTitle  = CreateFontA(-14, 0, 0, 0, FW_BOLD,     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            g_hFontHdrSub    = CreateFontA(-11, 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            g_hFontCardTitle = CreateFontA(-16, 0, 0, 0, FW_BOLD,     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            g_hFontTimer     = CreateFontA(-30, 0, 0, 0, FW_BOLD,     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            g_hFontSection   = CreateFontA(-13, 0, 0, 0, FW_BOLD,     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            g_hFontBtnBig    = CreateFontA(-14, 0, 0, 0, FW_BOLD,     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            g_hFontUi        = CreateFontA(-13, 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            g_hFontUiBold    = CreateFontA(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            g_hFontSmall     = CreateFontA(-11, 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

            g_brBg     = CreateSolidBrush(CLR_BG);
            g_brHeader = CreateSolidBrush(CLR_HEADER);
            g_brCard   = CreateSolidBrush(CLR_CARD);
            g_brRow    = CreateSolidBrush(CLR_ROW);

            // Barra de titulo escura nativa (Win 10/11) + cantos arredondados (Win 11)
            BOOL useDark = TRUE;
            DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
            DWORD cornerPref = 2; // DWMWCP_ROUND
            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));
            break;
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
            if (g_brBg) DeleteObject(g_brBg);
            if (g_brHeader) DeleteObject(g_brHeader);
            if (g_brCard) DeleteObject(g_brCard);
            if (g_brRow) DeleteObject(g_brRow);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static HWND CreateDarkEdit(HWND hParent, HINSTANCE hInst, int x, int y, int w, int h, int id, BOOL numericOnly) {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_RIGHT | ES_AUTOHSCROLL;
    if (numericOnly) style |= ES_NUMBER;
    HWND hEdit = CreateWindowExA(0, "EDIT", "", style, x, y, w, h, hParent, (HMENU)(INT_PTR)id, hInst, NULL);
    SendMessageA(hEdit, WM_SETFONT, (WPARAM)g_hFontUiBold, TRUE);
    return hEdit;
}

static HWND CreateOwnerBtn(HWND hParent, HINSTANCE hInst, int x, int y, int w, int h, int id) {
    return CreateWindowExA(0, "BUTTON", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        x, y, w, h, hParent, (HMENU)(INT_PTR)id, hInst, NULL);
}

static void CreateMainControls(HWND hWnd, HINSTANCE hInst) {
    // Header: Salvar
    CreateOwnerBtn(hWnd, hInst, 12, 8, 68, 30, IDC_BTN_SAVE);

    // Botao Principal Vermelho Pill
    CreateOwnerBtn(hWnd, hInst, 18, 56, 504, 42, IDC_BTN_TOGGLE);

    // Abas Ponto 1 / Ponto 2
    CreateOwnerBtn(hWnd, hInst, 18,  106, 248, 34, IDC_TAB_P1);
    CreateOwnerBtn(hWnd, hInst, 274, 106, 248, 34, IDC_TAB_P2);

    // Card base (cx = 18, cy = 148, sx = 34, sw = 472)
    int cy = 148;
    int sx = 34;

    // Switch Ativar Ponto (topo direito do card)
    CreateOwnerBtn(hWnd, hInst, 456, cy + 12, 48, 26, IDC_SW_ENABLED);

    // Secao 1: Local do Clique (g1y = cy + 136 = 284)
    int g1y = cy + 136;
    CreateOwnerBtn(hWnd, hInst, 448, g1y + 8,  48, 26, IDC_SW_FIXED);
    CreateOwnerBtn(hWnd, hInst, 356, g1y + 47, 140, 32, IDC_BTN_PICK);

    // X: Edit + (-) + (+)
    CreateDarkEdit(hWnd, hInst, 360, g1y + 96, 62, 20, IDC_EDT_X, FALSE);
    CreateOwnerBtn(hWnd, hInst, 434, g1y + 92, 26, 26, IDC_BTN_X_DEC);
    CreateOwnerBtn(hWnd, hInst, 468, g1y + 92, 26, 26, IDC_BTN_X_INC);

    // Y: Edit + (-) + (+)
    CreateDarkEdit(hWnd, hInst, 360, g1y + 138, 62, 20, IDC_EDT_Y, FALSE);
    CreateOwnerBtn(hWnd, hInst, 434, g1y + 134, 26, 26, IDC_BTN_Y_DEC);
    CreateOwnerBtn(hWnd, hInst, 468, g1y + 134, 26, 26, IDC_BTN_Y_INC);

    // Secao 2: Intervalo de Tempo (g2y = cy + 336 = 484)
    int g2y = cy + 336;
    CreateDarkEdit(hWnd, hInst, 360, g2y + 10, 62, 20, IDC_EDT_MIN, TRUE);
    CreateOwnerBtn(hWnd, hInst, 434, g2y + 6,  26, 26, IDC_BTN_MIN_DEC);
    CreateOwnerBtn(hWnd, hInst, 468, g2y + 6,  26, 26, IDC_BTN_MIN_INC);

    CreateDarkEdit(hWnd, hInst, 360, g2y + 48, 62, 20, IDC_EDT_SEC, TRUE);
    CreateOwnerBtn(hWnd, hInst, 434, g2y + 44, 26, 26, IDC_BTN_SEC_DEC);
    CreateOwnerBtn(hWnd, hInst, 468, g2y + 44, 26, 26, IDC_BTN_SEC_INC);

    // Presets Rapidos (y = cy + 418 = 566)
    int py = cy + 418;
    int pw = 64;
    for (int i = 0; i < 7; i++) {
        CreateOwnerBtn(hWnd, hInst, sx + i * 68, py, pw, 28, IDC_PRESET_5S + i);
    }

    // Secao 3: Opcoes do Botao (g3y = cy + 476 = 624)
    int g3y = cy + 476;
    CreateOwnerBtn(hWnd, hInst, 386, g3y + 4,  108, 26, IDC_BTN_MBTN);
    CreateOwnerBtn(hWnd, hInst, 448, g3y + 38, 48,  26, IDC_SW_DBL);

    // Botao Testar Clique (base do card)
    CreateOwnerBtn(hWnd, hInst, sx, cy + 552, 472, 32, IDC_BTN_TEST);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInst;
    (void)lpCmdLine;

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

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
        MessageBoxA(NULL, "Erro ao registrar classe da janela.", "AutoClicker", MB_ICONERROR);
        return 1;
    }

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rcWin = { 0, 0, WIN_CLIENT_W, WIN_CLIENT_H };
    AdjustWindowRect(&rcWin, style, FALSE);

    int winW = rcWin.right - rcWin.left;
    int winH = rcWin.bottom - rcWin.top;
    int posX = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
    int posY = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;
    if (posX < 0) posX = 40;
    if (posY < 0) posY = 20;

    g_hMainWnd = CreateWindowExA(
        0, "AutoClickerAdwaitaDarkClass", "AutoClicker Multi-Ponto",
        style,
        posX, posY, winW, winH,
        NULL, NULL, hInst, NULL
    );

    if (!g_hMainWnd) {
        MessageBoxA(NULL, "Erro ao criar a janela principal.", "AutoClicker", MB_ICONERROR);
        return 1;
    }

    CreateMainControls(g_hMainWnd, hInst);
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
