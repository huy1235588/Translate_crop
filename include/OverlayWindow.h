#ifndef OVERLAYWINDOW_H
#define OVERLAYWINDOW_H

#include <windows.h>
#include <string> // Include for std::wstring

// Khai báo hàm khởi tạo cửa sổ overlay
void CreateOverlayWindow(const std::wstring& outputFilename);

#endif // OVERLAYWINDOW_H