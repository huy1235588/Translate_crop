#ifndef OCR_H
#define OCR_H

#include <string>

// Hàm đọc văn bản từ một tệp hình ảnh
// Trả về văn bản đã trích xuất, hoặc chuỗi rỗng nếu thất bại.
std::wstring ReadTextFromImage(const std::wstring& imagePath);

#endif // OCR_H