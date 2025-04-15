#ifndef CROPSCREEN_H
#define CROPSCREEN_H

#include <windows.h>
#include <string> // Thêm include cho std::wstring

void CropScreen(POINT ptStart, POINT ptEnd, const std::wstring& outputFilePath);

#endif // CROPSCREEN_H
