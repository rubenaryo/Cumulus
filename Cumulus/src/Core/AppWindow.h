/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2019/12
Description : Definitions for OO-Window Creation
----------------------------------------------*/
#ifndef APPWINDOW_H
#define APPWINDOW_H

#include "WinApp.h"
#include <Core/Game.h>
//#include <combaseapi.h>

// A templated base class that other windows will inherit from
template <class WindowType>
class BaseWindow
{
public:
    // Static Window Procedure, who's functionality is inherited by all who inherit from this class
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        WindowType* pWindow = NULL;

        if (uMsg == WM_NCCREATE)
        {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pWindow = (WindowType*)pCreate->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pWindow);

            pWindow->m_hwnd = hwnd;
        }
        else
        {
            pWindow = (WindowType*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        }

        // Callback to our own message handler
        if (pWindow)
            return pWindow->HandleMessage(uMsg, wParam, lParam);
        else
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    // Default constructor initializes to NULL
    BaseWindow() : m_hwnd(NULL) {};

    virtual ~BaseWindow() {};

    // Method to create and register a WNDCLASS
    BOOL Create(PCWSTR lpWindowName, HINSTANCE hInstance, DWORD dwStyle, DWORD dwExStyle = 0, LONG x = 0, LONG y = 0, LONG nWidth = 800, LONG nHeight = 600, HWND hWndParent = 0, HMENU hMenu = 0);

    // Accessor for member HWND
    HWND Window() const { return m_hwnd; }

protected:
    // Handle to created window
    HWND m_hwnd;

    // Pure virtual functions that must be derived by any windows
    virtual PCWSTR ClassName() const = 0;
    virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;
    virtual bool InitGame(HWND hwnd, int width, int height) = 0;
};

// Definition of templated Create Method
template<class WindowType>
BOOL BaseWindow<WindowType>::Create(PCWSTR lpWindowName, HINSTANCE hInstance, DWORD dwStyle, DWORD dwExStyle, LONG x, LONG y, LONG nWidth, LONG nHeight, HWND hWndParent, HMENU hMenu)
{
    WNDCLASSEXW wc = {0};

    // Define WNDCLASS fields
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = dwStyle;
    wc.lpfnWndProc = WindowType::WindowProc;
    wc.hInstance = hInstance;
    //wc.hIcon = LoadIconW(hInstance, L"IDI_ICON");
    //wc.hIconSm = LoadIconW(wc.hInstance, L"IDI_ICON");
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW); // TODO: Import custom cursor
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = ClassName();

    if (!RegisterClassExW(&wc))
        return FALSE;
    
    RECT rc = { x, y, x + nWidth, y + nHeight};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    m_hwnd = CreateWindowExW(dwExStyle, ClassName(), lpWindowName, dwStyle, x, y, rc.right - rc.left, rc.bottom - rc.top, hWndParent, hMenu, hInstance, this);

    // Initialize Windows Imaging Component (WIC)
    if (FAILED(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED)))
        return FALSE;

    if (m_hwnd)
    {
        return InitGame(m_hwnd, nWidth, nHeight);
    }
    else
    {
        return FALSE;
    }
}
#endif