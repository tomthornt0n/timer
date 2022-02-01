#include <windows.h>
#include <strsafe.h>

#include <stdio.h>

LRESULT CALLBACK WindowProc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param);

RECT g_client_rect = { 0, 0, 600, 400 };
HINSTANCE g_instance_handle;

static size_t start_time = 0;

typedef enum
{
    AppState_Input,
    AppState_Timing,
    AppState_Done,
} AppState;
static AppState g_app_state = AppState_Input;

static HFONT g_font = 0;

typedef union
{
    struct
    {
        wchar_t hours[2];
        wchar_t colon_1;
        wchar_t minutes[2];
        wchar_t colon_2;
        wchar_t seconds[2];
    };
    wchar_t str[9];
} TimerStr;

static size_t
MicrosecondsFromTimerStr(TimerStr timer)
{
    for(int i = 0; i < 8; i += 1)
    {
        if(!('0' <= timer.str[i] && timer.str[i] <= '9'))
        {
            timer.str[i] = '0';
        }
    }
    
    int hours = (timer.hours[0] - '0')*10 + (timer.hours[1] - '0');
    int minutes = (timer.minutes[0] - '0')*10 + (timer.minutes[1] - '0');
    int seconds = (timer.seconds[0] - '0')*10 + (timer.seconds[1] - '0');
    
    hours = (hours < 0) ? 0 : hours;
    minutes = (minutes < 0) ? 0 : ((minutes > 60) ? 60 : minutes);
    seconds = (seconds < 0) ? 0 : ((seconds > 60) ? 60 : seconds);
    
    size_t result = seconds*1000000UL + minutes*60UL*1000000UL + hours*60UL*60UL*1000000UL;
    return result;
};

static TimerStr
TimerStrFromMicroseconds(size_t microseconds)
{
    size_t seconds = microseconds / 1000000UL;
    size_t minutes = seconds / 60UL;
    size_t hours = minutes / 60UL;
    
    TimerStr result = {0};
    _snwprintf(result.str, sizeof(result.str), L"%02zu:%02zu:%02zu", hours, minutes % 60, seconds % 60);
    
    return result;
}

static size_t g_perf_counter_ticks_per_second = 1;

static size_t
TimeInMicrosecondsGet(void)
{
    size_t result = 0;
    LARGE_INTEGER perf_counter;
    if(QueryPerformanceCounter(&perf_counter))
    {
        result = (perf_counter.QuadPart * 1000000llu) / g_perf_counter_ticks_per_second;
    }
    return result;
}

static size_t g_initial_time;

#define DefaultTimerStr L"__:__:__"
static TimerStr g_timer_entry = { .str = DefaultTimerStr };
static int g_timer_entry_cursor = 0;
static size_t g_timer_microseconds = 0;

static void
UpdateFont(void)
{
    if(0 != g_font)
    {
        DeleteObject(g_font);
    }
    g_font = CreateFontW(g_client_rect.bottom * 0.4f,
                         0, 0, 0, 400, 0, 0, 0,
                         ANSI_CHARSET,
                         OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY,
                         FF_MODERN,
                         L"Segoe UI");
}

static void
UpdateAndRender(HDC dc)
{
    HFONT old_font = SelectObject(dc, g_font);
    
    int x = g_client_rect.right / 2;
    int y = g_client_rect.bottom * 0.55f;
    
    if(AppState_Timing == g_app_state)
    {
        size_t t = TimeInMicrosecondsGet() - g_initial_time;
        if(g_timer_microseconds > t)
        {
            size_t remaining = g_timer_microseconds - t;
            TimerStr timer = TimerStrFromMicroseconds(remaining);
            TextOutW(dc, x, y, timer.str, 8);
        }
        else
        {
            PlaySoundW(L"gong", g_instance_handle, SND_RESOURCE | SND_ASYNC);
            g_app_state = AppState_Done;
            TextOutW(dc, x, y, DefaultTimerStr, 8);
        }
    }
    else if(AppState_Input == g_app_state)
    {
        TextOutW(dc, x, y, g_timer_entry.str, 8);
    }
    else if(AppState_Done == g_app_state)
    {
        TextOutW(dc, x, y, L"Time's up!", 10);
    }
    SelectObject(dc, old_font);
}

int WINAPI
WinMain(HINSTANCE instance_handle,
        HINSTANCE prev_instance_handle,
        LPSTR command_line,
        int show_mode)
{
    g_instance_handle = instance_handle;
    
    LARGE_INTEGER perf_counter_frequency;
    if(QueryPerformanceFrequency(&perf_counter_frequency))
    {
        g_perf_counter_ticks_per_second = perf_counter_frequency.QuadPart;
    }
    
    wchar_t *window_class_name = L"ClassTimer";
    
    WNDCLASSEX window_class =
    {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = WindowProc,
        .hInstance = instance_handle,
        .hCursor = LoadCursorW(NULL, IDC_ARROW),
        .hIcon = LoadIconW(instance_handle, L"mainIcon"),
        .hbrBackground = (HBRUSH)COLOR_WINDOW,
        .lpszClassName = window_class_name,
    };
    RegisterClassEx(&window_class);
    
    DWORD window_styles = WS_POPUPWINDOW | WS_CAPTION | WS_THICKFRAME;
    RECT window_rect = g_client_rect;
    AdjustWindowRect(&window_rect, window_styles, FALSE);
    
    HWND window_handle = CreateWindow(window_class_name,
                                      L"Class Timer",
                                      window_styles,
                                      CW_USEDEFAULT, CW_USEDEFAULT,
                                      window_rect.right - window_rect.left,
                                      window_rect.bottom - window_rect.top,
                                      NULL,
                                      NULL,
                                      instance_handle,
                                      NULL);
    SetWindowPos(window_handle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    ShowWindow(window_handle, SW_SHOW);
    
    HDC dc = GetDC(window_handle);
    
    UpdateFont();
    SetTextAlign(dc, TA_CENTER | TA_BASELINE);
    SetBkColor(dc, 0x00000000);
    SetTextColor(dc, 0x00aaaaaa);
    FillRect(dc, &g_client_rect, (HBRUSH)(COLOR_WINDOW + 3));
    
    g_initial_time = TimeInMicrosecondsGet();
    
    int exit_code;
    BOOL running = 1;
    while(running)
    {
        UpdateAndRender(dc);
        
        MSG msg = {0};
        if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if(msg.message  == WM_QUIT)
            {
                running = 0;
                exit_code = msg.wParam;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    ReleaseDC(window_handle, dc);
    
    return exit_code;
}

LRESULT CALLBACK
WindowProc(HWND window_handle,
           UINT message,
           WPARAM w_param,
           LPARAM l_param)
{
    if(WM_DESTROY == message)
    {
        PostQuitMessage(0);
        return 0;
    }
    else if(WM_SIZE == message)
    {
        GetClientRect(window_handle, &g_client_rect);
        UpdateFont();
        return 0;
    }
    else if (WM_PAINT == message)
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(window_handle, &ps);
        SetTextAlign(dc, TA_CENTER | TA_BASELINE);
        SetBkColor(dc, 0x00000000);
        SetTextColor(dc, 0x00aaaaaa);
        FillRect(dc, &g_client_rect, (HBRUSH)(COLOR_WINDOW + 3));
        UpdateAndRender(dc);
        return EndPaint(window_handle, &ps);
    }
    else if(WM_CHAR == message)
    {
        if(AppState_Input == g_app_state)
        {
            if('0' <= w_param && w_param <= '9')
            {
                g_timer_entry.str[g_timer_entry_cursor] = w_param;
                g_timer_entry_cursor += 1;
                if(g_timer_entry_cursor == 2 || g_timer_entry_cursor == 5)
                {
                    g_timer_entry_cursor += 1;
                }
            }
            else if('\r' == w_param)
            {
                g_app_state = AppState_Timing;
                g_initial_time = TimeInMicrosecondsGet();
                g_timer_microseconds = MicrosecondsFromTimerStr(g_timer_entry);
            }
            else if(8 == w_param && g_timer_entry_cursor > 0)
            {
                g_timer_entry_cursor -= 1;
                if(g_timer_entry_cursor == 2 || g_timer_entry_cursor == 5)
                {
                    g_timer_entry_cursor -= 1;
                }
                g_timer_entry.str[g_timer_entry_cursor] = '_';
            }
        }
        else if(AppState_Done == g_app_state)
        {
            if('\r' == w_param)
            {
                g_app_state = AppState_Input;
                g_timer_entry_cursor = 0;
                memcpy(g_timer_entry.str, DefaultTimerStr, sizeof(DefaultTimerStr));
            }
        }
        RedrawWindow(window_handle, NULL, NULL, RDW_INVALIDATE);
        return 0;
    }
    else if(WM_LBUTTONDOWN == message)
    {
        return 0;
    }
    else
    {
        return DefWindowProc(window_handle, message, w_param, l_param);
    }
}

