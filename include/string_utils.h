#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <string>
#include <windows.h>  // For MAX_PATH and Windows API functions
#include <filesystem> // For std::filesystem::path

// Hàm để lấy đường dẫn thư mục chứa file exe
std::string getExecutableDir();

// Hàm trợ giúp để chuyển std::wstring sang chuỗi UTF-8 std::string
std::string WstringToUtf8(const std::wstring &wstr);

// Hàm trợ giúp để chuyển chuỗi UTF-8 std::string sang std::wstring
std::wstring Utf8ToWstring(const std::string &str);

// Hàm trợ giúp để chuyển chuỗi std::wstring sang std::string (sử dụng mã hóa hệ thống mặc định)
// Chỉ sử dụng cho mục đích thông báo lỗi, không dùng trong OCR
std::string WstringToString(const std::wstring &wstr);

// Hàm trợ giúp để chuyển std::string sang std::wstring (sử dụng mã hóa hệ thống mặc định)
std::wstring StringToWstring(const std::string &str);

#endif // STRING_UTILS_H