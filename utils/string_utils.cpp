#include "../include/string_utils.h"

// Hàm để lấy đường dẫn thư mục chứa file exe
std::string getExecutableDir()
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return std::filesystem::path(path).parent_path().string();
}

// Hàm trợ giúp để chuyển std::wstring sang chuỗi UTF-8 std::string
std::string WstringToUtf8(const std::wstring &wstr)
{
    if (wstr.empty())
        return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    if (size_needed <= 0) return std::string(); // Handle error
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Hàm trợ giúp để chuyển chuỗi UTF-8 std::string sang std::wstring
std::wstring Utf8ToWstring(const std::string &str)
{
    if (str.empty())
        return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
     if (size_needed <= 0) return std::wstring(); // Handle error
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// Hàm trợ giúp để chuyển chuỗi std::wstring sang std::string
// Sử dụng mã hóa hệ thống mặc định (CP_ACP) cho thông báo lỗi đơn giản.
// Lưu ý: Hàm này khác với WstringToUtf8.
std::string WstringToString(const std::wstring &wstr)
{
    if (wstr.empty())
        return std::string();
    // Sử dụng CP_ACP thay vì CP_UTF8 cho mục đích thông báo lỗi đơn giản
    int size_needed = WideCharToMultiByte(CP_ACP, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
     if (size_needed <= 0) return std::string(); // Handle error
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_ACP, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Hàm trợ giúp để chuyển std::string sang std::wstring
// Sử dụng mã hóa hệ thống mặc định (CP_ACP) cho thông báo lỗi đơn giản.
// Lưu ý: Hàm này khác với Utf8ToWstring.
std::wstring StringToWstring(const std::string &str)
{
    if (str.empty())
        return std::wstring();
    // Sử dụng CP_ACP thay vì CP_UTF8 cho mục đích thông báo lỗi đơn giản
    int size_needed = MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), NULL, 0);
     if (size_needed <= 0) return std::wstring(); // Handle error
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}