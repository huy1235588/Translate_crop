#define UNICODE
#define _UNICODE

#include "../include/OverlayWindow.h"
#include "../include/CropScreen.h" // Bao gồm header cho CropAndTranslate
#include <windows.h>
#include <windowsx.h>               // Cho GET_X_LPARAM, GET_Y_LPARAM
#include <thread>                   // Để sử dụng std::thread nếu cần
#include <algorithm>                // Để sử dụng std::min và std::max
#include <string>                   // Include for std::wstring
#include <gdiplus.h>                // Cần thiết nếu sử dụng GDI+ trực tiếp ở đây
#pragma comment(lib, "gdiplus.lib") // Liên kết thư viện GDI+

// Biến toàn cục tĩnh
static POINT ptStart, ptEnd;
static bool isSelecting = false;
static std::wstring g_outputFilename; // Biến tĩnh để lưu tên file

// Màu khóa để tạo vùng trong suốt hoàn toàn bên trong hình chữ nhật
const COLORREF TRANSPARENT_COLOR_KEY = RGB(255, 0, 255); // Fuchsia

// Hàm lấy tọa độ đã chọn (không thay đổi)
void GetSelectionPoints(POINT *pStart, POINT *pEnd)
{
    *pStart = ptStart;
    *pEnd = ptEnd;
}

/**
 * @brief Thủ tục xử lý thông điệp cho cửa sổ overlay
 */
LRESULT CALLBACK OverlayWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    int xPos = GET_X_LPARAM(lParam);
    int yPos = GET_Y_LPARAM(lParam);

    switch (uMsg)
    {
    case WM_LBUTTONDOWN:
        isSelecting = true;
        ptStart.x = xPos;
        ptStart.y = yPos;
        ptEnd = ptStart;
        SetCapture(hwnd);
        // FALSE để không xóa nền, vì chúng ta sẽ vẽ lại hoàn toàn trong WM_PAINT
        InvalidateRect(hwnd, NULL, FALSE);
        break;

    case WM_MOUSEMOVE:
        if (isSelecting)
        {
            ptEnd.x = xPos;
            ptEnd.y = yPos;
            // FALSE để không xóa nền, giảm nhấp nháy
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;

    case WM_LBUTTONUP:
        if (isSelecting)
        {
            isSelecting = false;
            ReleaseCapture();
            ptEnd.x = xPos;
            ptEnd.y = yPos;

            int width = std::abs(ptEnd.x - ptStart.x);
            int height = std::abs(ptEnd.y - ptStart.y);

            // Lưu lại tọa độ trước khi đóng cửa sổ
            POINT start = ptStart;
            POINT end = ptEnd;
            std::wstring filename = g_outputFilename; // Sao chép tên file

            DestroyWindow(hwnd); // Đóng trước khi crop

            if (width > 0 && height > 0)
            {
                // Tạo luồng mới để crop ảnh
                std::thread cropThread([start, end, filename]()
                {
                    // Gọi hàm crop với tọa độ đã chọn và tên file
                    CropScreen(start, end, filename);
                });
                cropThread.detach(); // Tách luồng để không chặn
            }
        }
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        int screenWidth = clientRect.right;
        int screenHeight = clientRect.bottom;

        // --- Double Buffering ---
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, screenWidth, screenHeight);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

        // 1. Vẽ nền mờ (màu đen sẽ thành xám mờ do LWA_ALPHA)
        // Màu này KHÔNG được là TRANSPARENT_COLOR_KEY
        HBRUSH hBackgroundBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdcMem, &clientRect, hBackgroundBrush);
        DeleteObject(hBackgroundBrush);

        // 2. Nếu đang chọn, tạo "lỗ thủng" và vẽ viền trắng
        if (isSelecting)
        {
            // Xác định tọa độ hình chữ nhật
            RECT rect;
            rect.left = std::min(ptStart.x, ptEnd.x);
            rect.top = std::min(ptStart.y, ptEnd.y);
            rect.right = std::max(ptStart.x, ptEnd.x);
            rect.bottom = std::max(ptStart.y, ptEnd.y);

            // Chỉ vẽ nếu kích thước hợp lệ
            if (rect.right > rect.left && rect.bottom > rect.top)
            {
                // 2a. Tô màu khóa vào bên trong hình chữ nhật -> vùng này sẽ trong suốt hoàn toàn
                HBRUSH hKeyBrush = CreateSolidBrush(TRANSPARENT_COLOR_KEY);
                FillRect(hdcMem, &rect, hKeyBrush);
                DeleteObject(hKeyBrush);

                // 2b. Vẽ đường viền màu trắng (sẽ hơi mờ do LWA_ALPHA)
                HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255)); // Bút trắng
                HPEN hOldPen = (HPEN)SelectObject(hdcMem, hPen);
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcMem, GetStockObject(NULL_BRUSH)); // Không tô nền

                Rectangle(hdcMem, rect.left, rect.top, rect.right, rect.bottom);

                SelectObject(hdcMem, hOldPen);
                SelectObject(hdcMem, hOldBrush);
                DeleteObject(hPen);
            }
        }

        // 3. Sao chép từ memory DC ra màn hình
        BitBlt(hdc, 0, 0, screenWidth, screenHeight, hdcMem, 0, 0, SRCCOPY);

        // Dọn dẹp memory DC
        SelectObject(hdcMem, hbmOld);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            // if (isSelecting) // Luôn đóng khi nhấn ESC
            // {
            //     isSelecting = false;
            //     ReleaseCapture();
            //     InvalidateRect(hwnd, NULL, TRUE); // Vẽ lại để xóa hình chữ nhật
            // } else {
            DestroyWindow(hwnd);
            // }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

/**
 * @brief Tạo và hiển thị cửa sổ overlay toàn màn hình để chọn vùng
 * @param outputFilename Tên file để lưu ảnh đã crop
 * @note Hàm này sẽ chặn cho đến khi người dùng chọn xong hoặc hủy bỏ
 */
void CreateOverlayWindow(const std::wstring &outputFilename)
{
    // Lưu tên file vào biến tĩnh để OverlayWindowProc có thể truy cập
    g_outputFilename = outputFilename;

    // GDI+ nên được khởi tạo/dọn dẹp bằng RAII ở phạm vi rộng hơn nếu cần
    // Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    // ULONG_PTR gdiplusToken;
    // Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASS wc = {0};
    wc.lpfnWndProc = OverlayWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"OverlayWindowClass";
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);
    // Không cần wc.hbrBackground khi dùng WS_EX_LAYERED và vẽ thủ công

    RegisterClass(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    HWND hwndOverlay = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        wc.lpszClassName,
        L"Screen Crop Overlay",
        WS_POPUP, // Quan trọng: Không viền, không thanh tiêu đề
        0, 0, screenWidth, screenHeight,
        NULL, NULL, hInstance, NULL);

    if (!hwndOverlay)
    {
        MessageBox(NULL, L"Không thể tạo cửa sổ overlay!", L"Lỗi", MB_ICONERROR);
        // Gdiplus::GdiplusShutdown(gdiplusToken);
        return;
    }

    // --- Thiết lập độ trong suốt ---
    // Giá trị alpha tổng thể (ví dụ: 15% đục, 85% trong suốt)
    BYTE alphaValue = (255 * 60) / 100;
    // Đặt cả độ mờ tổng thể (LWA_ALPHA) và màu khóa (LWA_COLORKEY)
    // Những pixel có màu TRANSPARENT_COLOR_KEY sẽ trong suốt hoàn toàn.
    // Những pixel khác sẽ có độ mờ là alphaValue.
    SetLayeredWindowAttributes(hwndOverlay, TRANSPARENT_COLOR_KEY, alphaValue, LWA_ALPHA | LWA_COLORKEY);

    ShowWindow(hwndOverlay, SW_SHOWMAXIMIZED); // Hiển thị toàn màn hình
    UpdateWindow(hwndOverlay);

    // Reset trạng thái
    isSelecting = false;

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Gdiplus::GdiplusShutdown(gdiplusToken);
    // UnregisterClass(wc.lpszClassName, hInstance);
}