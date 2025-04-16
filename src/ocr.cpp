#include "../include/ocr.h"
#include <tesseract/baseapi.h>    // Tesseract API
#include <leptonica/allheaders.h> // Leptonica để đọc hình ảnh
#include <iostream>
#include <string>
#include <filesystem> // C++17 để làm việc với đường dẫn tệp
#include <stdexcept>
#include <windows.h> // Cho WideCharToMultiByte và MultiByteToWideChar

// Hàm để lấy đường dẫn thư mục chứa file exe
std::string getExecutableDir() {
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
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// Hàm trợ giúp để chuyển chuỗi std::wstring sang std::string
// Chỉ sử dụng cho mục đích thông báo lỗi, không dùng trong OCR
std::string WstringToString(const std::wstring &wstr)
{
    if (wstr.empty())
        return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Hàm để đọc văn bản từ một tệp hình ảnh sử dụng Tesseract
// Trả về văn bản đã trích xuất, hoặc chuỗi rỗng nếu thất bại.
std::wstring ReadTextFromImage(const std::wstring &imagePath)
{
    // Chuyển đổi đường dẫn std::wstring sang chuỗi char* mã hóa UTF-8 cho Tesseract/Leptonica
    std::string imagePathUtf8 = WstringToUtf8(imagePath);
    if (imagePathUtf8.empty())
    {
        std::wcerr << L"Lỗi: Đường dẫn tệp hình ảnh không hợp lệ hoặc không thể chuyển đổi sang UTF-8." << std::endl;
        return L"";
    }

    Pix *image = pixRead(imagePathUtf8.c_str());
    if (!image)
    {
        std::wcerr << L"Lỗi: Không thể mở hoặc đọc tệp hình ảnh: " << imagePath << std::endl;
        return L""; // Trả về chuỗi rỗng khi có lỗi
    }

    // Đường dẫn tới thư mục chứa tệp tessdata
    std::string executableDir = getExecutableDir();
    std::string tessdataDir = executableDir + "\\tessdata"; // Đường dẫn tới thư mục tessdata

    // Khởi tạo Tesseract API
    // Tham số đầu tiên là đường dẫn tới thư mục tessdata.
    // Tham số thứ hai là mã ngôn ngữ (ví dụ: "eng" cho tiếng Anh, "vie" cho tiếng Việt).
    tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();
    // Sử dụng "vie" cho tiếng Việt, "eng" cho tiếng Anh, hoặc "vie+eng" cho cả hai
    if (api->Init(tessdataDir.c_str(), "vie+eng"))
    {
        std::wcerr << L"Lỗi: Không thể khởi tạo Tesseract." << std::endl;
        pixDestroy(&image);
        delete api;
        return L"";
    }

    // Thiết lập hình ảnh cho Tesseract xử lý
    api->SetImage(image);

    // Thực hiện nhận dạng ký tự quang học (OCR)
    char *outText = api->GetUTF8Text();
    if (!outText)
    {
        std::wcerr << L"Lỗi: Tesseract không thể nhận dạng văn bản." << std::endl;
        api->End(); // Kết thúc phiên làm việc của Tesseract
        delete api;
        pixDestroy(&image); // Dọn dẹp hình ảnh của Leptonica
        return L"";
    }

    // Chuyển kết quả UTF-8 về std::wstring
    std::wstring extractedText = Utf8ToWstring(outText);

    // Dọn dẹp tài nguyên của Tesseract và Leptonica
    api->End(); // Kết thúc phiên làm việc của Tesseract
    delete api;
    delete[] outText;   // Xóa bộ đệm văn bản được cấp phát bởi Tesseract
    pixDestroy(&image); // Dọn dẹp hình ảnh của Leptonica

    // Trả về văn bản đã trích xuất
    return extractedText;
}
