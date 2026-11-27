/*
 * AutoClicker Multi-Ponto para Windows
 * Versão Nativa Win32 (64-bit) com suporte a múltiplos monitores,
 * seleção na tela por captura direta, tempos independentes (X e Y),
 * atalho global F6 e salvamento automático de opções.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#define IDI_APP_ICON 101
#define TIMER_TICK   1001

// IDs de controles
#define IDC_BTN_TOGGLE       2001
#define IDC_BTN_SAVE         2002
#define IDC_CHK_SOUND        2003

#define IDC_P1_ENABLED       2101
#define IDC_P1_FIXED         2102
#define IDC_P1_BTN_PICK      2103
#define IDC_P1_EDT_X         2104
#define IDC_P1_EDT_Y         2105
#define IDC_P1_EDT_MIN       2106
#define IDC_P1_EDT_SEC       2107
#define IDC_P1_CMB_BTN       2108
#define IDC_P1_CHK_DBL       2109
#define IDC_P1_BTN_TEST      2110
#define IDC_P1_LBL_COUNT     2111

#define IDC_P2_ENABLED       2201
#define IDC_P2_FIXED         2202
#define IDC_P2_BTN_PICK      2203
#define IDC_P2_EDT_X         2204
#define IDC_P2_EDT_Y         2205
#define IDC_P2_EDT_MIN       2206
#define IDC_P2_EDT_SEC       2207
#define IDC_P2_CMB_BTN       2208
#define IDC_P2_CHK_DBL       2209
#define IDC_P2_BTN_TEST      2210
#define IDC_P2_LBL_COUNT     2211

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
    int total_clicks;
} TargetConfig;

static TargetConfig g_p1 = { TRUE, TRUE, 500, 400, 2, 0, 0, FALSE, 0, 0 };
static TargetConfig g_p2 = { TRUE, TRUE, 800, 600, 5, 0, 0, FALSE, 0, 0 };

static BOOL g_is_running = FALSE;
static BOOL g_play_sound = TRUE;
static HWND g_hMainWnd = NULL;
static HFONT g_hFont = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontBig = NULL;

static char g_ini_path[MAX_PATH] = {0};
static int g_picking_target = 0; // 0=Nenhum, 1=P1, 2=P2

void LoadConfig(void);
void SaveConfig(void);
void ExecuteClick(TargetConfig *t);
void UpdateUIFromConfig(void);
void SyncConfigFromUI(void);
void ToggleRunning(void);

void InitIniPath(void) {
    char appdata[MAX_PATH];
    if (GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH) > 0) {
        wsprintfA(g_ini_path, "%s\\AutoClicker", appdata);
        CreateDirectoryA(g_ini_path, NULL);
        wsprintfA(g_ini_path, "%s\\AutoClicker\\config.ini", appdata);
    } else {
        lstrcpyA(g_ini_path, ".\\autoclicker_config.ini");
    }
}

void LoadConfig(void) {
    InitIniPath();
    g_play_sound   = GetPrivateProfileIntA("General", "PlaySound", 1, g_ini_path);

    // Ponto 1
    g_p1.enabled      = GetPrivateProfileIntA("Point1", "Enabled", 1, g_ini_path);
    g_p1.use_fixed    = GetPrivateProfileIntA("Point1", "UseFixed", 1, g_ini_path);
    g_p1.x            = (int)GetPrivateProfileIntA("Point1", "X", 500, g_ini_path);
    g_p1.y            = (int)GetPrivateProfileIntA("Point1", "Y", 400, g_ini_path);
    g_p1.minutes      = GetPrivateProfileIntA("Point1", "Minutes", 2, g_ini_path);
    g_p1.seconds      = GetPrivateProfileIntA("Point1", "Seconds", 0, g_ini_path);
    g_p1.button       = GetPrivateProfileIntA("Point1", "Button", 0, g_ini_path);
    g_p1.double_click = GetPrivateProfileIntA("Point1", "DoubleClick", 0, g_ini_path);

    // Ponto 2
    g_p2.enabled      = GetPrivateProfileIntA("Point2", "Enabled", 1, g_ini_path);
    g_p2.use_fixed    = GetPrivateProfileIntA("Point2", "UseFixed", 1, g_ini_path);
    g_p2.x            = (int)GetPrivateProfileIntA("Point2", "X", 800, g_ini_path);
    g_p2.y            = (int)GetPrivateProfileIntA("Point2", "Y", 600, g_ini_path);
    g_p2.minutes      = GetPrivateProfileIntA("Point2", "Minutes", 5, g_ini_path);
    g_p2.seconds      = GetPrivateProfileIntA("Point2", "Seconds", 0, g_ini_path);
    g_p2.button       = GetPrivateProfileIntA("Point2", "Button", 0, g_ini_path);
    g_p2.double_click = GetPrivateProfileIntA("Point2", "DoubleClick", 0, g_ini_path);
}

void SaveConfig(void) {
    char buf[64];
    InitIniPath();

    wsprintfA(buf, "%d", g_play_sound);
    WritePrivateProfileStringA("General", "PlaySound", buf, g_ini_path);

    // Ponto 1
    wsprintfA(buf, "%d", g_p1.enabled);
    WritePrivateProfileStringA("Point1", "Enabled", buf, g_ini_path);
    wsprintfA(buf, "%d", g_p1.use_fixed);
    WritePrivateProfileStringA("Point1", "UseFixed", buf, g_ini_path);
    wsprintfA(buf, "%d", g_p1.x);
    WritePrivateProfileStringA("Point1", "X", buf, g_ini_path);
    wsprintfA(buf, "%d", g_p1.y);
    WritePrivateProfileStringA("Point1", "Y", buf, g_ini_path);
    wsprintfA(buf, "%d", g_p1.minutes);
    WritePrivateProfileStringA("Point1", "Minutes", buf, g_ini_path);
    wsprintfA(buf, "%d", g_p1.seconds);
    WritePrivateProfileStringA("Point1", "Seconds", buf, g_ini_path);
    wsprintfA(buf, "%d", g_p1.button);
    WritePrivateProfileStringA("Point1", "Button", buf, g_ini_path);
    wsprintfA(buf, "%d", g_p1.double_click);
    WritePrivateProfileStringA("Point1", "DoubleClick", buf, g_ini_path);

    // Ponto 2
    wsprintfA(buf, "%d", g_p2.enabled);
    WritePrivateProfileStringA("Point2", "Enabled", buf, g_ini_path);
    wsprintfA(buf, "%d", g_p2.use_fixed);
    WritePrivateProfileStringA("Point2", "UseFixed", buf, g_ini_path);
    wsprintfA(buf, "%d", g_p2.x);
    WritePrivateProfileStringA("Point2", "X", buf, g_ini_path);
    wsprintfA(buf, "%d", g_p2.y);
    WritePrivateProfileStringA("Point2", "Y", buf, g_ini_path);
    wsprintfA(buf, "%d", g_p2.minutes);
    WritePrivateProfileStringA("Point2", "Minutes", buf, g_ini_path);
    wsprintfA(buf, "%d", g_p2.seconds);
    WritePrivateProfileStringA("Point2", "Seconds", buf, g_ini_path);
    wsprintfA(buf, "%d", g_p2.button);
    WritePrivateProfileStringA("Point2", "Button", buf, g_ini_path);
    wsprintfA(buf, "%d", g_p2.double_click);
    WritePrivateProfileStringA("Point2", "DoubleClick", buf, g_ini_path);
}

void DoMouseAction(DWORD flags) {
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    SendInput(1, &input, sizeof(INPUT));
}

void ExecuteClick(TargetConfig *t) {
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

void StartCapture(int target_id) {
    g_picking_target = target_id;
    SetCapture(g_hMainWnd);
    SetCursor(LoadCursorA(NULL, IDC_CROSS));

    if (target_id == 1) {
        SetDlgItemTextA(g_hMainWnd, IDC_P1_BTN_PICK, "Clique no alvo...");
    } else if (target_id == 2) {
        SetDlgItemTextA(g_hMainWnd, IDC_P2_BTN_PICK, "Clique no alvo...");
    }
}

void CancelCapture(void) {
    if (g_picking_target > 0) {
        g_picking_target = 0;
        ReleaseCapture();
        SetCursor(LoadCursorA(NULL, IDC_ARROW));
        SetDlgItemTextA(g_hMainWnd, IDC_P1_BTN_PICK, "[*] Marcar na Tela");
        SetDlgItemTextA(g_hMainWnd, IDC_P2_BTN_PICK, "[*] Marcar na Tela");
    }
}

void FinishCapture(int x, int y) {
    if (g_picking_target == 1) {
        g_p1.x = x;
        g_p1.y = y;
        g_p1.use_fixed = TRUE;
    } else if (g_picking_target == 2) {
        g_p2.x = x;
        g_p2.y = y;
        g_p2.use_fixed = TRUE;
    }
    CancelCapture();
    UpdateUIFromConfig();
    SaveConfig();
}

void UpdateUIFromConfig(void) {
    if (!g_hMainWnd) return;

    CheckDlgButton(g_hMainWnd, IDC_CHK_SOUND, g_play_sound ? BST_CHECKED : BST_UNCHECKED);

    // Ponto 1
    CheckDlgButton(g_hMainWnd, IDC_P1_ENABLED, g_p1.enabled ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(g_hMainWnd, IDC_P1_FIXED, g_p1.use_fixed ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(g_hMainWnd, IDC_P1_EDT_X, g_p1.x, TRUE);
    SetDlgItemInt(g_hMainWnd, IDC_P1_EDT_Y, g_p1.y, TRUE);
    SetDlgItemInt(g_hMainWnd, IDC_P1_EDT_MIN, g_p1.minutes, FALSE);
    SetDlgItemInt(g_hMainWnd, IDC_P1_EDT_SEC, g_p1.seconds, FALSE);
    SendDlgItemMessageA(g_hMainWnd, IDC_P1_CMB_BTN, CB_SETCURSEL, g_p1.button, 0);
    CheckDlgButton(g_hMainWnd, IDC_P1_CHK_DBL, g_p1.double_click ? BST_CHECKED : BST_UNCHECKED);

    // Ponto 2
    CheckDlgButton(g_hMainWnd, IDC_P2_ENABLED, g_p2.enabled ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(g_hMainWnd, IDC_P2_FIXED, g_p2.use_fixed ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(g_hMainWnd, IDC_P2_EDT_X, g_p2.x, TRUE);
    SetDlgItemInt(g_hMainWnd, IDC_P2_EDT_Y, g_p2.y, TRUE);
    SetDlgItemInt(g_hMainWnd, IDC_P2_EDT_MIN, g_p2.minutes, FALSE);
    SetDlgItemInt(g_hMainWnd, IDC_P2_EDT_SEC, g_p2.seconds, FALSE);
    SendDlgItemMessageA(g_hMainWnd, IDC_P2_CMB_BTN, CB_SETCURSEL, g_p2.button, 0);
    CheckDlgButton(g_hMainWnd, IDC_P2_CHK_DBL, g_p2.double_click ? BST_CHECKED : BST_UNCHECKED);

    char buf[128];
    wsprintfA(buf, "Proximo: %02d:%02d (%d cliques)", g_p1.minutes, g_p1.seconds, g_p1.total_clicks);
    SetDlgItemTextA(g_hMainWnd, IDC_P1_LBL_COUNT, buf);

    wsprintfA(buf, "Proximo: %02d:%02d (%d cliques)", g_p2.minutes, g_p2.seconds, g_p2.total_clicks);
    SetDlgItemTextA(g_hMainWnd, IDC_P2_LBL_COUNT, buf);
}

void SyncConfigFromUI(void) {
    if (!g_hMainWnd) return;

    g_play_sound = (IsDlgButtonChecked(g_hMainWnd, IDC_CHK_SOUND) == BST_CHECKED);

    // P1
    g_p1.enabled      = (IsDlgButtonChecked(g_hMainWnd, IDC_P1_ENABLED) == BST_CHECKED);
    g_p1.use_fixed    = (IsDlgButtonChecked(g_hMainWnd, IDC_P1_FIXED) == BST_CHECKED);
    g_p1.x            = (int)GetDlgItemInt(g_hMainWnd, IDC_P1_EDT_X, NULL, TRUE);
    g_p1.y            = (int)GetDlgItemInt(g_hMainWnd, IDC_P1_EDT_Y, NULL, TRUE);
    g_p1.minutes      = GetDlgItemInt(g_hMainWnd, IDC_P1_EDT_MIN, NULL, FALSE);
    g_p1.seconds      = GetDlgItemInt(g_hMainWnd, IDC_P1_EDT_SEC, NULL, FALSE);
    g_p1.button       = (int)SendDlgItemMessageA(g_hMainWnd, IDC_P1_CMB_BTN, CB_GETCURSEL, 0, 0);
    g_p1.double_click = (IsDlgButtonChecked(g_hMainWnd, IDC_P1_CHK_DBL) == BST_CHECKED);

    // P2
    g_p2.enabled      = (IsDlgButtonChecked(g_hMainWnd, IDC_P2_ENABLED) == BST_CHECKED);
    g_p2.use_fixed    = (IsDlgButtonChecked(g_hMainWnd, IDC_P2_FIXED) == BST_CHECKED);
    g_p2.x            = (int)GetDlgItemInt(g_hMainWnd, IDC_P2_EDT_X, NULL, TRUE);
    g_p2.y            = (int)GetDlgItemInt(g_hMainWnd, IDC_P2_EDT_Y, NULL, TRUE);
    g_p2.minutes      = GetDlgItemInt(g_hMainWnd, IDC_P2_EDT_MIN, NULL, FALSE);
    g_p2.seconds      = GetDlgItemInt(g_hMainWnd, IDC_P2_EDT_SEC, NULL, FALSE);
    g_p2.button       = (int)SendDlgItemMessageA(g_hMainWnd, IDC_P2_CMB_BTN, CB_GETCURSEL, 0, 0);
    g_p2.double_click = (IsDlgButtonChecked(g_hMainWnd, IDC_P2_CHK_DBL) == BST_CHECKED);
}

void ToggleRunning(void) {
    SyncConfigFromUI();
    g_is_running = !g_is_running;

    if (g_is_running) {
        DWORD now = GetTickCount();
        DWORD interval_p1 = (g_p1.minutes * 60 + g_p1.seconds) * 1000;
        DWORD interval_p2 = (g_p2.minutes * 60 + g_p2.seconds) * 1000;
        if (interval_p1 < 200) interval_p1 = 200;
        if (interval_p2 < 200) interval_p2 = 200;

        g_p1.next_tick = now + interval_p1;
        g_p2.next_tick = now + interval_p2;

        SetTimer(g_hMainWnd, TIMER_TICK, 100, NULL);
        SetDlgItemTextA(g_hMainWnd, IDC_BTN_TOGGLE, "[X] PARAR AUTOCLICKER");
    } else {
        KillTimer(g_hMainWnd, TIMER_TICK);
        SetDlgItemTextA(g_hMainWnd, IDC_BTN_TOGGLE, "[>] INICIAR AUTOCLICKER");
        UpdateUIFromConfig();
    }
}

// Procedimento de janela
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hFont = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            g_hFontBold = CreateFontA(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            g_hFontBig = CreateFontA(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

            break;
        }

        case WM_LBUTTONDOWN:
            if (g_picking_target > 0) {
                POINT pt;
                GetCursorPos(&pt);
                FinishCapture(pt.x, pt.y);
                return 0;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE && g_picking_target > 0) {
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

            if (id == IDC_BTN_TOGGLE) {
                ToggleRunning();
            } else if (id == IDC_BTN_SAVE) {
                SyncConfigFromUI();
                SaveConfig();
                MessageBoxA(hwnd, "Configuracoes salvas com sucesso em %APPDATA%\\AutoClicker\\config.ini!", "AutoClicker", MB_OK | MB_ICONINFORMATION);
            } else if (id == IDC_P1_BTN_PICK) {
                StartCapture(1);
            } else if (id == IDC_P2_BTN_PICK) {
                StartCapture(2);
            } else if (id == IDC_P1_BTN_TEST) {
                SyncConfigFromUI();
                ExecuteClick(&g_p1);
                UpdateUIFromConfig();
            } else if (id == IDC_P2_BTN_TEST) {
                SyncConfigFromUI();
                ExecuteClick(&g_p2);
                UpdateUIFromConfig();
            } else if (code == EN_CHANGE || code == CBN_SELCHANGE || code == BN_CLICKED) {
                SyncConfigFromUI();
            }
            break;
        }

        case WM_TIMER: {
            if (!g_is_running) break;

            DWORD now = GetTickCount();

            // P1
            if (g_p1.enabled) {
                DWORD interval = (g_p1.minutes * 60 + g_p1.seconds) * 1000;
                if (interval < 200) interval = 200;

                if (now >= g_p1.next_tick) {
                    ExecuteClick(&g_p1);
                    g_p1.next_tick = now + interval;
                }
                int rem_sec = (g_p1.next_tick > now) ? (int)((g_p1.next_tick - now) / 1000) : 0;
                char buf[128];
                wsprintfA(buf, "Proximo: %02d:%02d (%d cliques)", rem_sec / 60, rem_sec % 60, g_p1.total_clicks);
                SetDlgItemTextA(hwnd, IDC_P1_LBL_COUNT, buf);
            }

            // P2
            if (g_p2.enabled) {
                DWORD interval = (g_p2.minutes * 60 + g_p2.seconds) * 1000;
                if (interval < 200) interval = 200;

                if (now >= g_p2.next_tick) {
                    ExecuteClick(&g_p2);
                    g_p2.next_tick = now + interval;
                }
                int rem_sec = (g_p2.next_tick > now) ? (int)((g_p2.next_tick - now) / 1000) : 0;
                char buf[128];
                wsprintfA(buf, "Proximo: %02d:%02d (%d cliques)", rem_sec / 60, rem_sec % 60, g_p2.total_clicks);
                SetDlgItemTextA(hwnd, IDC_P2_LBL_COUNT, buf);
            }
            break;
        }

        case WM_CLOSE:
            SyncConfigFromUI();
            SaveConfig();
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static BOOL CALLBACK SetChildFontProc(HWND hChild, LPARAM lp) {
    if (hChild != (HWND)lp) {
        SendMessageA(hChild, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    }
    return TRUE;
}

// Criação de controles
void BuildControls(HWND hWnd, HINSTANCE hInst) {
    // Botão Iniciar/Parar
    HWND hBtnToggle = CreateWindowExA(0, "BUTTON", "[>] INICIAR AUTOCLICKER",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        20, 16, 500, 48, hWnd, (HMENU)IDC_BTN_TOGGLE, hInst, NULL);
    SendMessageA(hBtnToggle, WM_SETFONT, (WPARAM)g_hFontBig, TRUE);

    // ==========================================
    // GRUPO PONTO 1
    // ==========================================
    HWND hGrp1 = CreateWindowExA(0, "BUTTON", " PONTO 1 (TEMPO X) ",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        20, 76, 500, 205, hWnd, NULL, hInst, NULL);
    SendMessageA(hGrp1, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    CreateWindowExA(0, "BUTTON", "Ativar Ponto 1",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 102, 120, 22, hWnd, (HMENU)IDC_P1_ENABLED, hInst, NULL);

    CreateWindowExA(0, "BUTTON", "Usar Posicao Fixa",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        170, 102, 140, 22, hWnd, (HMENU)IDC_P1_FIXED, hInst, NULL);

    CreateWindowExA(0, "BUTTON", "[*] Marcar na Tela",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        330, 98, 170, 28, hWnd, (HMENU)IDC_P1_BTN_PICK, hInst, NULL);

    CreateWindowExA(0, "STATIC", "Coordenada X:", WS_CHILD | WS_VISIBLE,
        35, 136, 95, 20, hWnd, NULL, hInst, NULL);
    CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "500", WS_CHILD | WS_VISIBLE,
        135, 134, 75, 24, hWnd, (HMENU)IDC_P1_EDT_X, hInst, NULL);

    CreateWindowExA(0, "STATIC", "Coordenada Y:", WS_CHILD | WS_VISIBLE,
        230, 136, 95, 20, hWnd, NULL, hInst, NULL);
    CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "400", WS_CHILD | WS_VISIBLE,
        330, 134, 75, 24, hWnd, (HMENU)IDC_P1_EDT_Y, hInst, NULL);

    CreateWindowExA(0, "STATIC", "Tempo: Minutos:", WS_CHILD | WS_VISIBLE,
        35, 170, 95, 20, hWnd, NULL, hInst, NULL);
    CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "2", WS_CHILD | WS_VISIBLE,
        135, 168, 50, 24, hWnd, (HMENU)IDC_P1_EDT_MIN, hInst, NULL);

    CreateWindowExA(0, "STATIC", "Segundos:", WS_CHILD | WS_VISIBLE,
        230, 170, 70, 20, hWnd, NULL, hInst, NULL);
    CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "0", WS_CHILD | WS_VISIBLE,
        305, 168, 50, 24, hWnd, (HMENU)IDC_P1_EDT_SEC, hInst, NULL);

    CreateWindowExA(0, "STATIC", "Botao:", WS_CHILD | WS_VISIBLE,
        35, 204, 50, 20, hWnd, NULL, hInst, NULL);
    HWND hCmb1 = CreateWindowExA(0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        90, 202, 120, 120, hWnd, (HMENU)IDC_P1_CMB_BTN, hInst, NULL);
    SendMessageA(hCmb1, CB_ADDSTRING, 0, (LPARAM)"Esquerdo");
    SendMessageA(hCmb1, CB_ADDSTRING, 0, (LPARAM)"Direito");
    SendMessageA(hCmb1, CB_ADDSTRING, 0, (LPARAM)"Meio");
    SendMessageA(hCmb1, CB_SETCURSEL, 0, 0);

    CreateWindowExA(0, "BUTTON", "Clique Duplo",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        230, 204, 110, 22, hWnd, (HMENU)IDC_P1_CHK_DBL, hInst, NULL);

    CreateWindowExA(0, "BUTTON", "Testar P1",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        370, 198, 130, 28, hWnd, (HMENU)IDC_P1_BTN_TEST, hInst, NULL);

    HWND hLbl1 = CreateWindowExA(0, "STATIC", "Proximo: 02:00 (0 cliques)", WS_CHILD | WS_VISIBLE,
        35, 242, 450, 22, hWnd, (HMENU)IDC_P1_LBL_COUNT, hInst, NULL);
    SendMessageA(hLbl1, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    // ==========================================
    // GRUPO PONTO 2
    // ==========================================
    HWND hGrp2 = CreateWindowExA(0, "BUTTON", " PONTO 2 (TEMPO Y) ",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        20, 292, 500, 205, hWnd, NULL, hInst, NULL);
    SendMessageA(hGrp2, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    CreateWindowExA(0, "BUTTON", "Ativar Ponto 2",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 318, 120, 22, hWnd, (HMENU)IDC_P2_ENABLED, hInst, NULL);

    CreateWindowExA(0, "BUTTON", "Usar Posicao Fixa",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        170, 318, 140, 22, hWnd, (HMENU)IDC_P2_FIXED, hInst, NULL);

    CreateWindowExA(0, "BUTTON", "[*] Marcar na Tela",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        330, 314, 170, 28, hWnd, (HMENU)IDC_P2_BTN_PICK, hInst, NULL);

    CreateWindowExA(0, "STATIC", "Coordenada X:", WS_CHILD | WS_VISIBLE,
        35, 352, 95, 20, hWnd, NULL, hInst, NULL);
    CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "800", WS_CHILD | WS_VISIBLE,
        135, 350, 75, 24, hWnd, (HMENU)IDC_P2_EDT_X, hInst, NULL);

    CreateWindowExA(0, "STATIC", "Coordenada Y:", WS_CHILD | WS_VISIBLE,
        230, 352, 95, 20, hWnd, NULL, hInst, NULL);
    CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "600", WS_CHILD | WS_VISIBLE,
        330, 350, 75, 24, hWnd, (HMENU)IDC_P2_EDT_Y, hInst, NULL);

    CreateWindowExA(0, "STATIC", "Tempo: Minutos:", WS_CHILD | WS_VISIBLE,
        35, 386, 95, 20, hWnd, NULL, hInst, NULL);
    CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "5", WS_CHILD | WS_VISIBLE,
        135, 384, 50, 24, hWnd, (HMENU)IDC_P2_EDT_MIN, hInst, NULL);

    CreateWindowExA(0, "STATIC", "Segundos:", WS_CHILD | WS_VISIBLE,
        230, 386, 70, 20, hWnd, NULL, hInst, NULL);
    CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "0", WS_CHILD | WS_VISIBLE,
        305, 384, 50, 24, hWnd, (HMENU)IDC_P2_EDT_SEC, hInst, NULL);

    CreateWindowExA(0, "STATIC", "Botao:", WS_CHILD | WS_VISIBLE,
        35, 420, 50, 20, hWnd, NULL, hInst, NULL);
    HWND hCmb2 = CreateWindowExA(0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        90, 418, 120, 120, hWnd, (HMENU)IDC_P2_CMB_BTN, hInst, NULL);
    SendMessageA(hCmb2, CB_ADDSTRING, 0, (LPARAM)"Esquerdo");
    SendMessageA(hCmb2, CB_ADDSTRING, 0, (LPARAM)"Direito");
    SendMessageA(hCmb2, CB_ADDSTRING, 0, (LPARAM)"Meio");
    SendMessageA(hCmb2, CB_SETCURSEL, 0, 0);

    CreateWindowExA(0, "BUTTON", "Clique Duplo",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        230, 420, 110, 22, hWnd, (HMENU)IDC_P2_CHK_DBL, hInst, NULL);

    CreateWindowExA(0, "BUTTON", "Testar P2",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        370, 414, 130, 28, hWnd, (HMENU)IDC_P2_BTN_TEST, hInst, NULL);

    HWND hLbl2 = CreateWindowExA(0, "STATIC", "Proximo: 05:00 (0 cliques)", WS_CHILD | WS_VISIBLE,
        35, 458, 450, 22, hWnd, (HMENU)IDC_P2_LBL_COUNT, hInst, NULL);
    SendMessageA(hLbl2, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    // ==========================================
    // PAINEL INFERIOR
    // ==========================================
    CreateWindowExA(0, "BUTTON", "Som de Notificacao",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 514, 160, 26, hWnd, (HMENU)IDC_CHK_SOUND, hInst, NULL);

    CreateWindowExA(0, "BUTTON", "Salvar Configuracoes",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        320, 510, 200, 32, hWnd, (HMENU)IDC_BTN_SAVE, hInst, NULL);

    EnumChildWindows(hWnd, SetChildFontProc, (LPARAM)hBtnToggle);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    LoadConfig();

    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = "AutoClickerMainClass";
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.hIcon         = LoadIconA(hInst, MAKEINTRESOURCEA(IDI_APP_ICON));
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

    if (!RegisterClassA(&wc)) {
        return 1;
    }

    int winWidth  = 560;
    int winHeight = 600;
    int scrWidth  = GetSystemMetrics(SM_CXSCREEN);
    int scrHeight = GetSystemMetrics(SM_CYSCREEN);
    int winX      = (scrWidth - winWidth) / 2;
    int winY      = (scrHeight - winHeight) / 2;

    HWND hWnd = CreateWindowExA(
        0,
        "AutoClickerMainClass",
        "AutoClicker Multi-Ponto (Windows)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        winX, winY, winWidth, winHeight,
        NULL, NULL, hInst, NULL
    );

    if (!hWnd) return 1;
    g_hMainWnd = hWnd;

    BuildControls(hWnd, hInst);
    UpdateUIFromConfig();

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return (int)msg.wParam;
}

// Ponto de entrada nativo do Windows PE x64 (substitui o CRT da Microsoft)
void WinMainCRTStartup(void) {
    HINSTANCE hInst = GetModuleHandleA(NULL);
    STARTUPINFOA si;
    si.cb = sizeof(si);
    GetStartupInfoA(&si);
    int nCmdShow = (si.dwFlags & STARTF_USESHOWWINDOW) ? si.wShowWindow : SW_SHOWNORMAL;
    LPSTR lpCmdLine = GetCommandLineA();
    int ret = WinMain(hInst, NULL, lpCmdLine, nCmdShow);
    ExitProcess(ret);
}
