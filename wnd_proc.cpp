#include <tchar.h>
#include <vector>
#include <algorithm>
#include <random>
#include <windows.h>
#include <windowsx.h>

#include "main.h"
#include "wnd_proc.h"

static BOOL OnCreate(HWND, LPCREATESTRUCT);                                         // WM_CREATE
static void OnCommand(HWND, int, HWND, UINT);                                      	// WM_COMMAND
static void OnPaint(HWND);                                                        	// WM_PAINT
static void OnLButtonDown(HWND, BOOL, int, int, UINT);                              // WM_LBUTTONDOWN
static void OnDrawItem(HWND, const DRAWITEMSTRUCT *);                               // WM_DRAWITEM
static void OnTimer(HWND, UINT);                                                    // WM_TIMER
static void OnDestroy(HWND);                                                        // WM_DESTROY

#define IDC_NEW_GAME 1000
#define IDC_GAME_MODE1 1001
#define IDC_GAME_MODE2 1002
#define IDC_GAME_LEVEL1 1003
#define IDC_GAME_LEVEL2 1004
#define IDC_NEXT_ROUND 1005

static HBRUSH hBackgroundBrush;
static HPEN hBoardPen, hFxPen, hF0Pen, hWinPen;


static HFONT hQueueFont; // Чия черга ходити.
static HFONT hCounterFont; // Лічильники.
static HFONT hButtonFont; // Шрифт на кнопках.
static HFONT hDefaultFont; // Шрифт, яким буде писатись все решта.

enum CELL { ID_EMPTY, ID_X, ID_0 };

// Всі значення з комірок ігрової дошки.
// 0 1 2
// 3 4 5
// 6 7 8
static std::vector<CELL> board(9);

// Хто ходить.
static CELL queue = ID_X;

// Лічильники.
static int iCounterX = 0; // Кількість виграшів Х.
static int iCounter0 = 0; // Кількість виграшів 0.
static int iCounter = 0; // Кількість ходів.

// 1 - гра на двох.
// 2 - гра з компютером.
static int iGameMode = 2;

// 1 - легкий рівень.
// 2 - складний рівень.
static int iGameLevel = 1;

// Колір фону повідомлення про виграш.
static COLORREF clrResBackColor = 0;


// [DrawLine]:
inline bool DrawLine(HDC hDC, int x0, int y0, int x1, int y1)
{
    MoveToEx(hDC, x0, y0, NULL);
    return LineTo(hDC, x1, y1);
}
// [/DrawLine]


// [DrawX]:
static void DrawX(HDC hDC, int x, int y, int size)
{
    DrawLine(hDC, x, y, x + size, y + size);
    DrawLine(hDC, x + size, y, x, y + size);
}
// [/DrawX]


// [Draw0]:
static void Draw0(HDC hDC, int x, int y, int size)
{
    HBRUSH hOldBrush = SelectBrush(hDC, GetStockBrush(NULL_BRUSH));
    Ellipse(hDC, x, y, x + size, y + size);
    SelectBrush(hDC, hOldBrush);
}
// [/Draw0]


// [DrawF]:
static void DrawF(HDC hDC, int x, int y, int size, int i)
{
    if (board[i] == ID_X)
    {
        HPEN hOldPen = SelectPen(hDC, hFxPen);
        DrawX(hDC, x, y, size);
        SelectPen(hDC, hOldPen);
    }
    else if (board[i] == ID_0)
    {
        HPEN hOldPen = SelectPen(hDC, hF0Pen);
        Draw0(hDC, x, y, size);
        SelectPen(hDC, hOldPen);
    }
}
// [/DrawF]


// [DrawStaticForm]:
static void DrawStaticForm(HDC hDC, int x, int y, int width, int height, const TCHAR *szText)
{
    DrawLine(hDC, x, y, x, y + height);
    DrawLine(hDC, x + width, y, x + width, y + height);
    DrawLine(hDC, x, y + height, x + width + 1, y + height);

    HFONT hOldFont = SelectFont(hDC, hDefaultFont);

    SIZE size;
    GetTextExtentPoint32(hDC, szText, lstrlen(szText), &size);
    int xT = (width >> 1) - (size.cx >> 1);
    int yT = size.cy >> 1;

    TextOut(hDC, x + xT, y - yT, szText, lstrlen(szText));

    SelectFont(hDC, hOldFont);

    DrawLine(hDC, x, y, x + xT - 5, y);
    DrawLine(hDC, x + width + 1 - xT + 5, y, x + width + 1, y);
}
// [/DrawStaticForm]


// [GameMode2Level1]: знаходження 0 в легкому рівні.
static void GameMode2Level1()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 8);

    int rand0 = dis(gen);
    if (board[rand0] == ID_EMPTY)
    {
        queue = ID_X;
        iCounter++;
        board[rand0] = ID_0;
    }
    else
        GameMode2Level1();
}
// [/GameMode2Level1]


// [GameMode2Level2]: знаходження 0 в складному рівні.
static void GameMode2Level2()
{
    // Усі комбінації.
    const int cmap[24][3] = {
        0, 1, 2,
        0, 2, 1,
        1, 2, 0,
        3, 4, 5,
        3, 5, 4,
        4, 5, 3,
        6, 7, 8,
        6, 8, 7,
        7, 8, 6,
        0, 3, 6,
        0, 6, 3,
        3, 6, 0,
        1, 4, 7,
        1, 7, 4,
        4, 7, 1,
        2, 5, 8,
        2, 8, 5,
        5, 8, 2,
        0, 4, 8,
        0, 8, 4,
        4, 8, 0,
        2, 4, 6,
        2, 6, 4,
        4, 6, 2
    };

    queue = ID_X;

    // Доставляєм виграшну комбінацію.
    for (int i = 0; i < 24; i++)
    {
        if (board[cmap[i][0]] == ID_0 && board[cmap[i][1]] == ID_0 && board[cmap[i][2]] == ID_EMPTY)
        {
            iCounter++;
            board[cmap[i][2]] = ID_0;
            return;
        }
    }

    // Якшо виграшної комбінації для '0' немає, а для 'X' є, то мішаєм виграти супротивнику.
    for (int i = 0; i < 24; i++)
    {
        if (board[cmap[i][0]] == ID_X && board[cmap[i][1]] == ID_X && board[cmap[i][2]] == ID_EMPTY)
        {
            iCounter++;
            board[cmap[i][2]] = ID_0;
            return;
        }
    }

    // Якщо немає ніякої виграшної комбінації, то знаходимо рандомне 0.
    GameMode2Level1();
}
// [/GameMode2Level2]


// [IsWin]: повертає хто виграв, і в яких комірках виграшна комбінація.
static CELL IsWin(int cells[3])
{
    const int winsmap[8][3] = {
        0, 1, 2,
        3, 4, 5,
        6, 7, 8,
        0, 3, 6,
        1, 4, 7,
        2, 5, 8,
        0, 4, 8,
        2, 4, 6
    };

    for (int i = 0; i < 8; i++)
    {
        if ((board[winsmap[i][0]] == ID_X && board[winsmap[i][1]] == ID_X && board[winsmap[i][2]] == ID_X) ||
            (board[winsmap[i][0]] == ID_0 && board[winsmap[i][1]] == ID_0 && board[winsmap[i][2]] == ID_0))
        {
            if (cells != NULL)
            {
                cells[0] = winsmap[i][0];
                cells[1] = winsmap[i][1];
                cells[2] = winsmap[i][2];
            }
            return board[winsmap[i][0]];
        }
    }
    return ID_EMPTY;
}
// [/IsWin]


// [UpdateButtons]:
static void UpdateButtons(HWND hWnd)
{
    InvalidateRect(GetDlgItem(hWnd, IDC_GAME_MODE1), NULL, TRUE);
    InvalidateRect(GetDlgItem(hWnd, IDC_GAME_MODE2), NULL, TRUE);
    InvalidateRect(GetDlgItem(hWnd, IDC_GAME_LEVEL1), NULL, TRUE);
    InvalidateRect(GetDlgItem(hWnd, IDC_GAME_LEVEL2), NULL, TRUE);
}
// [/UpdateButtons]


// [WindowProcedure]: 
LRESULT CALLBACK WindowProcedure(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        HANDLE_MSG(hWnd, WM_CREATE, OnCreate);
        HANDLE_MSG(hWnd, WM_COMMAND, OnCommand);
        HANDLE_MSG(hWnd, WM_PAINT, OnPaint);
        HANDLE_MSG(hWnd, WM_LBUTTONDOWN, OnLButtonDown);
        HANDLE_MSG(hWnd, WM_DRAWITEM, OnDrawItem);
        HANDLE_MSG(hWnd, WM_TIMER, OnTimer);
        HANDLE_MSG(hWnd, WM_DESTROY, OnDestroy);

    case WM_ERASEBKGND:
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
// [/WindowProcedure]


// [OnCreate]: WM_CREATE
static BOOL OnCreate(HWND hWnd, LPCREATESTRUCT lpcs)
{
    std::fill(board.begin(), board.end(), ID_EMPTY);

    hBackgroundBrush = CreateSolidBrush(RGB(150, 200, 230));
    hBoardPen = CreatePen(PS_SOLID, 2, RGB(50, 50, 50));
    hFxPen = CreatePen(PS_SOLID, 7, RGB(100, 150, 150));
    hF0Pen = CreatePen(PS_SOLID, 7, RGB(100, 150, 170));
    hWinPen = CreatePen(PS_SOLID, 20, RGB(230, 130, 130));

    hQueueFont = CreateFont(-30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, _T("Arial"));

    hCounterFont = CreateFont(-20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, _T("Arial"));

    hButtonFont = CreateFont(-15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, _T("Arial"));

    hDefaultFont = CreateFont(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, _T("Arial"));

    CreateWindowEx(0, _T("button"), _T("Нова ігра"), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 400, 10, 300, 30, hWnd, (HMENU)IDC_NEW_GAME, lpcs->hInstance, NULL);
    CreateWindowEx(0, _T("button"), _T("Гра на двох"), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 410, 180, 280, 30, hWnd, (HMENU)IDC_GAME_MODE1, lpcs->hInstance, NULL);
    CreateWindowEx(0, _T("button"), _T("Гра з компютером"), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 410, 220, 280, 30, hWnd, (HMENU)IDC_GAME_MODE2, lpcs->hInstance, NULL);
    CreateWindowEx(0, _T("button"), _T("Легкий"), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 450, 260, 240, 30, hWnd, (HMENU)IDC_GAME_LEVEL1, lpcs->hInstance, NULL);
    CreateWindowEx(0, _T("button"), _T("Складный"), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 450, 300, 240, 30, hWnd, (HMENU)IDC_GAME_LEVEL2, lpcs->hInstance, NULL);

    CreateWindowEx(0, _T("button"), _T("Продовжити"), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 100, 190, 180, 25, hWnd, (HMENU)IDC_NEXT_ROUND, lpcs->hInstance, NULL);

    SendMessage(GetDlgItem(hWnd, IDC_NEW_GAME), WM_SETFONT, (WPARAM)hButtonFont, 0L);
    SendMessage(GetDlgItem(hWnd, IDC_GAME_MODE1), WM_SETFONT, (WPARAM)hButtonFont, 0L);
    SendMessage(GetDlgItem(hWnd, IDC_GAME_MODE2), WM_SETFONT, (WPARAM)hButtonFont, 0L);
    SendMessage(GetDlgItem(hWnd, IDC_GAME_LEVEL1), WM_SETFONT, (WPARAM)hButtonFont, 0L);
    SendMessage(GetDlgItem(hWnd, IDC_GAME_LEVEL2), WM_SETFONT, (WPARAM)hButtonFont, 0L);
    SendMessage(GetDlgItem(hWnd, IDC_NEXT_ROUND), WM_SETFONT, (WPARAM)hDefaultFont, 0L);

    ShowWindow(GetDlgItem(hWnd, IDC_NEXT_ROUND), SW_HIDE);
    SetTimer(hWnd, 1, 100, NULL);
    return TRUE;
}
// [/OnCreate]


// [OnCommand]: WM_COMMAND
static void OnCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify)
{
    switch (id)
    {
    case IDC_GAME_MODE1:
    {
        iGameMode = 1;
        iGameLevel = 0;
    }
    break;

    case IDC_GAME_MODE2:
    {
        iGameMode = 2;
        iGameLevel = 1;
    }
    break;

    case IDC_GAME_LEVEL1:
    {
        iGameMode = 2;
        iGameLevel = 1;
    }
    break;

    case IDC_GAME_LEVEL2:
    {
        iGameMode = 2;
        iGameLevel = 2;
    }
    break;
    }

    if (id == IDC_NEXT_ROUND || id == IDC_NEW_GAME || id == IDC_GAME_MODE1 || id == IDC_GAME_MODE2 || id == IDC_GAME_LEVEL1 || id == IDC_GAME_LEVEL2)
    {
        ShowWindow(GetDlgItem(hWnd, IDC_NEXT_ROUND), SW_HIDE);
        std::fill(board.begin(), board.end(), ID_EMPTY);
        queue = ID_X;
        iCounter = 0;

        if (id != IDC_NEXT_ROUND)
            iCounterX = iCounter0 = 0;

        UpdateButtons(hWnd);
        InvalidateRect(hWnd, NULL, FALSE);
    }
}
// [/OnCommand]


// [OnPaint]: WM_PAINT
static void OnPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hDC = BeginPaint(hWnd, &ps);
    RECT rc;
    GetClientRect(hWnd, &rc);
    const int iWindowWidth = rc.right - rc.left;
    const int iWindowHeight = rc.bottom - rc.top;
    HDC hMemDC = CreateCompatibleDC(hDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hDC, iWindowWidth, iWindowHeight);
    HBITMAP hOldBitmap = SelectBitmap(hMemDC, hBitmap);
    FillRect(hMemDC, &ps.rcPaint, hBackgroundBrush);

    int iOldBkMode = SetBkMode(hMemDC, TRANSPARENT);

    HFONT hOldFont;
    HPEN hOldPen;
    SIZE size;
    COLORREF clrOldColor;
    TCHAR szText[1028] = { 0 };
    
    // Розмір комірки: 100x100
    // 10 + 10 + 100 + 10 + 10 + 100 + 10 + 10 + 100 + 10 = 370 (розмір всього ігрового поля)
    // 10 (відступ зліва) +
    // 10 (відступ до малюнка) + 100 (перший малюнок) + 10 (відступ до наступної комірки) +
    // 10 (відступ до малюнка) + 100 (другий малюнок) + 10 (відступ до наступної комірки) +
    // 10 (відступ до малюнка) + 100 (третій малюнок) + 10 (кінцевий відступ)

    // 10 + 10 + 100 + 10 = 130 (відступ до другої комірки)
    // 10 + 10 + 100 + 10 + 10 + 100 + 10 = 250 (відступ до третьої комірки)

    hOldPen = SelectPen(hMemDC, hBoardPen);
    DrawLine(hMemDC, 10, 130, 370, 130);
    DrawLine(hMemDC, 10, 250, 370, 250);
    DrawLine(hMemDC, 130, 10, 130, 370);
    DrawLine(hMemDC, 250, 10, 250, 370);
    SelectPen(hMemDC, hOldPen);

    int iCellSize = 100;
    DrawF(hMemDC, 20, 20, iCellSize, 0);
    DrawF(hMemDC, 140, 20, iCellSize, 1);
    DrawF(hMemDC, 260, 20, iCellSize, 2);
    DrawF(hMemDC, 20, 140, iCellSize, 3);
    DrawF(hMemDC, 140, 140, iCellSize, 4);
    DrawF(hMemDC, 260, 140, iCellSize, 5);
    DrawF(hMemDC, 20, 260, iCellSize, 6);
    DrawF(hMemDC, 140, 260, iCellSize, 7);
    DrawF(hMemDC, 260, 260, iCellSize, 8);

    RECT rcCells[9] = {
        20, 20, 120, 120,
        140, 20, 240, 120,
        260, 20, 360, 120,
        20, 140, 120, 240,
        140, 140, 240, 240,
        260, 140, 360, 240,
        20, 260, 120, 360,
        140, 260, 240, 360,
        260, 260, 360, 360
    };

    int cells[3];
    CELL is_win = IsWin(cells);
    if (is_win != ID_EMPTY)
    {
        hOldPen = SelectPen(hMemDC, hWinPen);

        int y = rcCells[cells[0]].top + ((rcCells[cells[0]].bottom - rcCells[cells[0]].top) >> 1);
        if ((cells[0] == 0 && cells[1] == 1 && cells[2] == 2) || (cells[0] == 3 && cells[1] == 4 && cells[2] == 5) || (cells[0] == 6 && cells[1] == 7 && cells[2] == 8))
            DrawLine(hMemDC, rcCells[cells[0]].left, y, rcCells[cells[2]].right, y);

        int x = rcCells[cells[0]].left + ((rcCells[cells[0]].right - rcCells[cells[0]].left) >> 1);
        if ((cells[0] == 0 && cells[1] == 3 && cells[2] == 6) || (cells[0] == 1 && cells[1] == 4 && cells[2] == 7) || (cells[0] == 2 && cells[1] == 5 && cells[2] == 8))
            DrawLine(hMemDC, x, rcCells[cells[0]].top, x, rcCells[cells[2]].bottom);

        if (cells[0] == 0 && cells[1] == 4 && cells[2] == 8)
            DrawLine(hMemDC, rcCells[cells[0]].left, rcCells[cells[0]].top, rcCells[cells[2]].right, rcCells[cells[2]].bottom);

        if (cells[0] == 2 && cells[1] == 4 && cells[2] == 6)
            DrawLine(hMemDC, rcCells[cells[0]].right, rcCells[cells[0]].top, rcCells[cells[2]].left, rcCells[cells[2]].bottom);

        SelectPen(hMemDC, hOldPen);
    }

    if (is_win != ID_EMPTY)
        wsprintf(szText, _T("Виграв %s!"), is_win == ID_X ? _T("X") : _T("0"));
    if (is_win == ID_EMPTY && iCounter == 9)
        lstrcpy(szText, _T("Нічия :-("));

    if (is_win != ID_EMPTY || iCounter == 9)
    {
        int center = 190; // 130 250
        int cw = 100;
        int ch = 35;

        HRGN hRgn = CreateRoundRectRgn(center - cw - 2, center - ch - 2, center + cw + 2, center + ch + 2, 20, 20);
        if (hRgn)
        {
            HBRUSH hBrush = CreateSolidBrush(clrResBackColor);
            FillRgn(hMemDC, hRgn, hBrush);// GetStockBrush(BLACK_BRUSH));
            DeleteBrush(hBrush);
            DeleteRgn(hRgn);
            hRgn = CreateRoundRectRgn(center - cw, center - ch, center + cw, center + ch, 20, 20);
            if (hRgn)
            {
                FillRgn(hMemDC, hRgn, GetStockBrush(DKGRAY_BRUSH));
                DeleteRgn(hRgn);
            }
        }

        clrOldColor = SetTextColor(hMemDC, RGB(230, 230, 230));
        hOldFont = SelectFont(hMemDC, hDefaultFont);
        GetTextExtentPoint32(hMemDC, szText, lstrlen(szText), &size);
        TextOut(hMemDC, 190 - (size.cx >> 1), 165, szText, lstrlen(szText));
        SelectFont(hMemDC, hOldFont);
        SetTextColor(hMemDC, clrOldColor);
    }

    DrawStaticForm(hMemDC, 400, 60, 110, 90, _T("Чий хід"));

    hOldFont = SelectFont(hMemDC, hQueueFont);
    clrOldColor = SetTextColor(hMemDC, RGB(50, 50, 200));

    lstrcpy(szText, queue == ID_X ? _T("X") : _T("0"));

    GetTextExtentPoint32(hMemDC, szText, lstrlen(szText), &size);
    int x = 400 + (110 >> 1) - (size.cx >> 1);
    int y = 60 + (90 >> 1) - (size.cy >> 1);

    TextOut(hMemDC, x, y, szText, lstrlen(szText));
    
    SetTextColor(hMemDC, clrOldColor);
    SelectFont(hMemDC, hOldFont);

    DrawStaticForm(hMemDC, 520, 60, 180, 90, _T("Лічильник виграшів"));

    hOldFont = SelectFont(hMemDC, hCounterFont);
    clrOldColor = SetTextColor(hMemDC, RGB(0, 0, 10));

    TextOut(hMemDC, 540, 80, _T("X"), 1);
    TextOut(hMemDC, 560, 80, _T(":"), 1);
    TextOut(hMemDC, 540, 110, _T("0"), 1);
    TextOut(hMemDC, 560, 110, _T(":"), 1);

    SetTextColor(hMemDC, RGB(0, 0, 100));

    wsprintf(szText, _T("%d"), iCounterX);
    TextOut(hMemDC, 580, 80, szText, lstrlen(szText));
    wsprintf(szText, _T("%d"), iCounter0);
    TextOut(hMemDC, 580, 110, szText, lstrlen(szText));

    SetTextColor(hMemDC, clrOldColor);
    SelectFont(hMemDC, hOldFont);

    DrawStaticForm(hMemDC, 400, 170, 300, 170, _T("Режими"));

    DrawLine(hMemDC, 440, 250, 440, 315);
    DrawLine(hMemDC, 440, 275, 450, 275);
    DrawLine(hMemDC, 440, 315, 450, 315);

    SetBkMode(hMemDC, iOldBkMode);

    BitBlt(hDC, 0, 0, iWindowWidth, iWindowHeight, hMemDC, 0, 0, SRCCOPY);
    SelectBitmap(hMemDC, hOldBitmap);
    DeleteBitmap(hBitmap);
    DeleteDC(hMemDC);
    EndPaint(hWnd, &ps);
}
// [/OnPaint]


// [OnLButtonDown]:
static void OnLButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
    RECT rc[9] = {
        20, 20, 120, 120,
        140, 20, 240, 120,
        260, 20, 360, 120,
        20, 140, 120, 240,
        140, 140, 240, 240,
        260, 140, 360, 240,
        20, 260, 120, 360,
        140, 260, 240, 360,
        260, 260, 360, 360
    };

    if (IsWin(NULL) == ID_EMPTY)
    {
        for (int i = 0; i < 9; i++)
        {
            if (PtInRect(&rc[i], { x, y }))
            {
                if (board[i] == ID_EMPTY)
                {
                    board[i] = queue;
                    queue = queue == ID_X ? ID_0 : ID_X;
                    iCounter++;

                    if (iCounter < 9 && iCounter >= 0 && IsWin(NULL) == ID_EMPTY)
                    {
                        if (iGameMode == 2 && iGameLevel == 1)
                            GameMode2Level1();
                        if (iGameMode == 2 && iGameLevel == 2)
                            GameMode2Level2();
                    }
                }
                else
                    MessageBox(hWnd, _T("Цей хід вже зроблений!"), _T("Сповіщення"), MB_OK | MB_ICONINFORMATION);
                break;
            }
        }

        CELL is_win = IsWin(NULL);
        if (is_win != ID_EMPTY)
        {
            is_win == ID_X ? iCounterX++ : iCounter0++;
            ShowWindow(GetDlgItem(hWnd, IDC_NEXT_ROUND), SW_SHOW);
        }

        if (iCounter == 9)
            ShowWindow(GetDlgItem(hWnd, IDC_NEXT_ROUND), SW_SHOW);

        /*
        CELL is_win = IsWin(NULL);
        if (is_win != ID_EMPTY)
        {
            is_win == ID_X ? iCounterX++ : iCounter0++;

            InvalidateRect(hWnd, NULL, FALSE);
            TCHAR szText[128] = { 0 };
            wsprintf(szText, _T("Виграв %s! Продовжити ігру?"), is_win == ID_X ? _T("X") : _T("0"));
            if (MessageBox(hWnd, szText, _T("Ура!"), MB_OKCANCEL | MB_ICONINFORMATION) == IDOK)
            {
                iCounter = 0;
                std::fill(board.begin(), board.end(), ID_EMPTY);
                queue = ID_X;
            }
        }

        if (iCounter == 9)
        {
            InvalidateRect(hWnd, NULL, FALSE);
            if (MessageBox(hWnd, _T("Нічия! Продовжити ігру?"), _T("Ура!"), MB_OKCANCEL | MB_ICONINFORMATION) == IDOK)
            {
                iCounter = 0;
                std::fill(board.begin(), board.end(), ID_EMPTY);
                queue = ID_X;
            }
        }
        */
    }

    InvalidateRect(hWnd, NULL, FALSE);
}
// [/OnLButtonDown]


// [OnDrawItem]: WM_DRAWITEM
static void OnDrawItem(HWND hWnd, const DRAWITEMSTRUCT *lpdis)
{
    if (lpdis->CtlType & ODT_BUTTON)
    {
        TCHAR szText[256] = { 0 };
        GetWindowText(lpdis->hwndItem, szText, 256);

        TEXTMETRIC tm;
        GetTextMetrics(lpdis->hDC, &tm);

        SIZE size;
        GetTextExtentPoint32(lpdis->hDC, szText, lstrlen(szText), &size);

        int iOldBkMode = SetBkMode(lpdis->hDC, TRANSPARENT);
        COLORREF clrOldColor = SetTextColor(lpdis->hDC, RGB(0, 0, 0));


        switch (lpdis->CtlID)
        {
        case IDC_NEW_GAME:
        case IDC_NEXT_ROUND:
        {
            HBRUSH hBrush = CreateSolidBrush(RGB(120, 150, 120));
            HBRUSH hOldBrush;
            if (lpdis->itemState & ODS_SELECTED)
                hOldBrush = SelectBrush(lpdis->hDC, GetStockBrush(DKGRAY_BRUSH));
            else
                hOldBrush = SelectBrush(lpdis->hDC, hBrush);
            Rectangle(lpdis->hDC, lpdis->rcItem.left, lpdis->rcItem.top, lpdis->rcItem.right, lpdis->rcItem.bottom);
            SelectBrush(lpdis->hDC, hOldBrush);
            DeleteBrush(hBrush);

            int x = (lpdis->rcItem.right + lpdis->rcItem.left - size.cx) / 2;
            int y = (lpdis->rcItem.bottom + lpdis->rcItem.top - tm.tmHeight) / 2;
            TextOut(lpdis->hDC, x, y, szText, lstrlen(szText));
        }
        break;

        case IDC_GAME_MODE1:
        case IDC_GAME_MODE2:
        case IDC_GAME_LEVEL1:
        case IDC_GAME_LEVEL2:
        {
            HBRUSH hOldBrush = SelectBrush(lpdis->hDC, CreateSolidBrush(RGB(120, 150, 120)));
            Rectangle(lpdis->hDC, lpdis->rcItem.left, lpdis->rcItem.top, lpdis->rcItem.right, lpdis->rcItem.bottom);
            DeleteBrush(SelectBrush(lpdis->hDC, hOldBrush));

            hOldBrush = SelectBrush(lpdis->hDC, GetStockBrush(GRAY_BRUSH));

            bool IsChecked = false;
            if (lpdis->CtlID == IDC_GAME_MODE1 && iGameMode == 1)
                IsChecked = true;
            if (lpdis->CtlID == IDC_GAME_MODE2 && iGameMode == 2)
                IsChecked = true;
            if (lpdis->CtlID == IDC_GAME_LEVEL1 && iGameLevel == 1)
                IsChecked = true;
            if (lpdis->CtlID == IDC_GAME_LEVEL2 && iGameLevel == 2)
                IsChecked = true;

            if (IsChecked)
                Ellipse(lpdis->hDC, lpdis->rcItem.left + 5, lpdis->rcItem.top + 5, lpdis->rcItem.left + 25, lpdis->rcItem.top + 25);
            SelectBrush(lpdis->hDC, hOldBrush);

            int x = lpdis->rcItem.left + 30;
            int y = (lpdis->rcItem.bottom + lpdis->rcItem.top - tm.tmHeight) / 2;
            TextOut(lpdis->hDC, x, y, szText, lstrlen(szText));
        }
        break;
        }

        SetBkMode(lpdis->hDC, iOldBkMode);
        SetTextColor(lpdis->hDC, clrOldColor);
    }
}
// [/OnDrawItem]


// [OnTimer]: WM_TIMER
static void OnTimer(HWND hWnd, UINT id)
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    const int iWindowWidth = rc.right - rc.left;
    const int iWindowHeight = rc.bottom - rc.top;

    switch (id)
    {
    case 1:
    {
        /*
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, RGB(255, 255, 255));

        clrResBackColor = dis(gen);
        */
        /*
        static int mode = 1;
        
        int k = GetRValue(clrResBackColor);

        if (k == 0)
            mode = 1;
        if (k == 250)
            mode = 2;

        if (mode == 1)
            k += 25;
        if (mode == 2)
            k -= 25;

        clrResBackColor = RGB(k, k, k);
        */
        static int gb = 1;
        static int i = 0;

        const int k[15] = { 50, 75, 100, 125, 150, 175, 200, 225, 200, 175, 150, 125, 100, 75, 50 };

        int a1 = 0, a2 = 0;

        if (i == 15)
        {
            i = 0;
            gb = gb == 1 ? 2 : 1;
        }

        if (gb == 1)
            a1 = k[i++];
        else
            a2 = k[i++];

        clrResBackColor = RGB(a1, a2, 0);

        RECT rc = { 0, 0, 370, 370 };
        InvalidateRect(hWnd, &rc, FALSE);
    }
    break;
    }
}
// [/OnTimer]


// [OnDestroy]: WM_DESTROY
static void OnDestroy(HWND hWnd)
{
    DeleteBrush(hBackgroundBrush);
    DeletePen(hBoardPen);
    DeletePen(hFxPen);
    DeletePen(hF0Pen);
    DeletePen(hWinPen);
    DeleteFont(hQueueFont);
    DeleteFont(hCounterFont);
    DeleteFont(hButtonFont);
    DeleteFont(hDefaultFont);
    KillTimer(hWnd, 1);
    PostQuitMessage(0);
}
// [/OnDestroy]
