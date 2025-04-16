#include "../include/CropScreen.h" // Consider renaming this include if you rename the header file
#include "../include/ocr.h" // Bao gồm header cho chức năng OCR
#include "../include/translate.h" // <--- Thêm include này
#include <windows.h>
#include <gdiplus.h> // Bao gồm header GDI+
#include <algorithm> // Cho std::min, std::max, std::abs
#include <iostream>  // Cho std::cerr, std::wcout
#include <memory>    // Cho std::unique_ptr
#include <string>    // Cho std::wstring
#include <stdexcept> // Cho std::runtime_error

#pragma comment(lib, "gdiplus.lib") // Liên kết với thư viện GDI+

// --- Các lớp RAII để quản lý tài nguyên ---

// Lớp RAII cho việc khởi tạo và dọn dẹp GDI+
class GdiplusInitializer
{
private:
    ULONG_PTR m_token;            // Token GDI+
    Gdiplus::Status m_initStatus; // Trạng thái khởi tạo
public:
    GdiplusInitializer() : m_token(0)
    {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        m_initStatus = Gdiplus::GdiplusStartup(&m_token, &gdiplusStartupInput, NULL);
    }
    ~GdiplusInitializer()
    {
        if (m_initStatus == Gdiplus::Ok)
        { // Chỉ shutdown nếu startup thành công
            Gdiplus::GdiplusShutdown(m_token);
        }
    }
    // Kiểm tra trạng thái khởi tạo
    bool IsInitialized() const
    {
        return m_initStatus == Gdiplus::Ok;
    }
    // Ngăn chặn sao chép và gán
    GdiplusInitializer(const GdiplusInitializer &) = delete;
    GdiplusInitializer &operator=(const GdiplusInitializer &) = delete;
};

// Lớp RAII cho Device Context (DC)
class DCWrapper
{
private:
    HDC m_hDC;
    HWND m_hWnd;       // Lưu lại HWND để dùng với ReleaseDC (nếu là Screen DC)
    bool m_isScreenDC; // Phân biệt Screen DC và Memory DC
public:
    // Constructor cho Screen DC (lấy từ HWND, NULL là toàn màn hình)
    DCWrapper(HWND hWnd) : m_hWnd(hWnd), m_isScreenDC(true)
    {
        m_hDC = GetDC(hWnd);
    }
    // Constructor cho Memory DC (tạo từ DC tương thích)
    DCWrapper(HDC hCompatibleDC) : m_hWnd(NULL), m_isScreenDC(false)
    {
        m_hDC = CreateCompatibleDC(hCompatibleDC);
    }
    ~DCWrapper()
    {
        if (m_hDC)
        {
            if (m_isScreenDC)
            {
                ReleaseDC(m_hWnd, m_hDC); // Giải phóng Screen DC
            }
            else
            {
                DeleteDC(m_hDC); // Xóa Memory DC
            }
        }
    }
    // Lấy handle HDC
    operator HDC() const { return m_hDC; }
    // Kiểm tra DC hợp lệ
    bool IsValid() const { return m_hDC != NULL; }
    // Ngăn chặn sao chép và gán
    DCWrapper(const DCWrapper &) = delete;
    DCWrapper &operator=(const DCWrapper &) = delete;
};

// Lớp RAII cho các đối tượng GDI (HBITMAP, HPEN, HBRUSH, ...)
template <typename T>
class GdiObjectWrapper
{
private:
    T m_handle;

public:
    GdiObjectWrapper(T handle = NULL) : m_handle(handle) {} // Cho phép khởi tạo rỗng
    ~GdiObjectWrapper()
    {
        if (m_handle)
        {
            DeleteObject(m_handle);
        }
    }
    // Gán handle mới (giải phóng handle cũ nếu có)
    GdiObjectWrapper &operator=(T newHandle)
    {
        if (m_handle && m_handle != newHandle)
        {
            DeleteObject(m_handle);
        }
        m_handle = newHandle;
        return *this;
    }
    // Lấy handle
    operator T() const { return m_handle; }
    // Kiểm tra handle hợp lệ
    bool IsValid() const { return m_handle != NULL; }
    // Giải phóng handle và đặt lại thành NULL
    T Detach()
    {
        T oldHandle = m_handle;
        m_handle = NULL;
        return oldHandle;
    }
    // Ngăn chặn sao chép và gán (hoặc có thể implement move semantics)
    GdiObjectWrapper(const GdiObjectWrapper &) = delete;
    GdiObjectWrapper &operator=(const GdiObjectWrapper &) = delete;
};

// --- Hàm trợ giúp ---

// Hàm lấy CLSID của bộ mã hóa hình ảnh (ví dụ: "image/png", "image/jpeg")
int GetEncoderClsid(const WCHAR *format, CLSID *pClsid)
{
    UINT num = 0;  // Số lượng bộ mã hóa
    UINT size = 0; // Kích thước buffer cần thiết

    Gdiplus::Status status = Gdiplus::GetImageEncodersSize(&num, &size);
    if (status != Gdiplus::Ok || size == 0)
    {
        return -1; // Lỗi hoặc không có bộ mã hóa nào
    }

    // Dùng unique_ptr để tự động quản lý bộ nhớ
    std::unique_ptr<BYTE[]> buffer(new BYTE[size]);
    if (!buffer)
    {
        return -1; // Không cấp phát được bộ nhớ
    }
    Gdiplus::ImageCodecInfo *pImageCodecInfo = reinterpret_cast<Gdiplus::ImageCodecInfo *>(buffer.get());

    status = Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
    if (status != Gdiplus::Ok)
    {
        return -1; // Lỗi khi lấy danh sách bộ mã hóa
    }

    // Tìm bộ mã hóa khớp với định dạng MIME yêu cầu
    for (UINT j = 0; j < num; ++j)
    {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            return j; // Trả về chỉ số của bộ mã hóa tìm thấy
        }
    }

    return -1; // Không tìm thấy bộ mã hóa phù hợp
}

// --- Chức năng chính ---

/**
 * @brief Chụp một vùng màn hình được xác định bởi ptStart và ptEnd, sau đó lưu thành file PNG.
 * @param ptStart Điểm bắt đầu (hoặc một góc của hình chữ nhật).
 * @param ptEnd Điểm kết thúc (hoặc góc đối diện của hình chữ nhật).
 * @param outputFilePath Đường dẫn để lưu file ảnh PNG đã crop.
 */
void CropScreen(POINT ptStart, POINT ptEnd, const std::wstring &outputFilePath)
{
    // Khởi tạo GDI+ một lần duy nhất cho hàm này bằng RAII
    GdiplusInitializer gdiplusInit;
    if (!gdiplusInit.IsInitialized())
    {
        std::wcerr << L"Lỗi: Không thể khởi tạo GDI+." << std::endl;
        // Consider throwing an exception or returning an error code instead of just printing
        return;
    }

    bool success = false; // Biến để theo dõi trạng thái thành công

    try
    {
        // 1. Xác định tọa độ và kích thước vùng crop (chuẩn hóa)
        int x = std::min(ptStart.x, ptEnd.x);
        int y = std::min(ptStart.y, ptEnd.y);
        int width = std::abs(ptEnd.x - ptStart.x);
        int height = std::abs(ptEnd.y - ptStart.y);

        // Kiểm tra kích thước hợp lệ
        if (width <= 0 || height <= 0)
        {
            throw std::runtime_error("Kích thước vùng crop không hợp lệ.");
        }

        // 2. Lấy Device Context (DC) của toàn màn hình
        DCWrapper hScreenDC((HWND)NULL); // Sử dụng RAII
        if (!hScreenDC.IsValid())
        {
            throw std::runtime_error("Không thể lấy DC màn hình.");
        }

        // 3. Tạo một Memory DC tương thích với Screen DC
        DCWrapper hMemoryDC(static_cast<HDC>(hScreenDC)); // Sử dụng RAII
        if (!hMemoryDC.IsValid())
        {
            throw std::runtime_error("Không thể tạo Memory DC.");
        }

        // 4. Tạo một Bitmap tương thích với Screen DC để chứa ảnh crop
        GdiObjectWrapper<HBITMAP> hBitmapWrapper; // Sử dụng RAII
        hBitmapWrapper = CreateCompatibleBitmap(hScreenDC, width, height);
        if (!hBitmapWrapper.IsValid())
        {
            throw std::runtime_error("Không thể tạo Bitmap.");
        }

        // 5. Chọn Bitmap vào Memory DC và lưu lại Bitmap cũ
        // An toàn hơn nếu quản lý bitmap cũ bằng RAII, hoặc đảm bảo luôn gọi SelectObject trước khi trả về hoặc ném ngoại lệ
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmapWrapper);
        // Việc sử dụng wrapper RAII để chọn/hủy chọn các đối tượng GDI có thể mang lại lợi ích ở đây

        // 6. Sao chép (BitBlt) dữ liệu hình ảnh từ Screen DC sang Memory DC
        if (!BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, x, y, SRCCOPY))
        {
            SelectObject(hMemoryDC, hOldBitmap); // Khôi phục bitmap cũ trước khi ném ngoại lệ
            throw std::runtime_error("Lỗi sao chép hình ảnh (BitBlt).");
        }

        // 7. Tạo đối tượng Gdiplus::Bitmap từ HBITMAP
        // Sử dụng unique_ptr để quản lý đối tượng Bitmap
        std::unique_ptr<Gdiplus::Bitmap> bitmap(Gdiplus::Bitmap::FromHBITMAP(hBitmapWrapper, NULL));
        if (!bitmap)
        {
            SelectObject(hMemoryDC, hOldBitmap); // Khôi phục bitmap cũ
            // Destructor của hBitmapWrapper sẽ xóa HBITMAP, điều này là đúng ở đây
            throw std::runtime_error("Không thể tạo Gdiplus::Bitmap từ HBITMAP.");
        }
        // Ngắt kết nối HBITMAP từ wrapper vì Gdiplus::Bitmap hiện sở hữu nó (hoặc chia sẻ quyền sở hữu tuỳ thuộc vào cài đặt)
        // Tuy nhiên, theo tài liệu, Gdiplus::Bitmap::FromHBITMAP KHÔNG nắm quyền sở hữu.
        // Vì vậy, HBITMAP cần được xóa sau khi Gdiplus::Bitmap đã sử dụng xong.
        // Wrapper RAII hiện tại xử lý điều này đúng cách khi hBitmapWrapper ra khỏi phạm vi sau khi bitmap được sử dụng.

        // 8. Lấy CLSID của bộ mã hóa PNG
        CLSID pngClsid;
        if (GetEncoderClsid(L"image/png", &pngClsid) < 0)
        {
            SelectObject(hMemoryDC, hOldBitmap); // Khôi phục bitmap cũ
            throw std::runtime_error("Không tìm thấy bộ mã hóa PNG.");
        }

        // 9. Lưu Bitmap thành file PNG using the provided path
        Gdiplus::Status status = bitmap->Save(outputFilePath.c_str(), &pngClsid, NULL);
        if (status != Gdiplus::Ok)
        {
            SelectObject(hMemoryDC, hOldBitmap); // Khôi phục bitmap cũ
            // Map Gdiplus::Status to a more descriptive error message if possible
            throw std::runtime_error("Không thể lưu hình ảnh PNG.");
        }

        std::wcout << L"Đã lưu ảnh crop vào: " << outputFilePath << std::endl;

        success = true; // Đánh dấu thành công nếu không có ngoại lệ nào xảy ra

        // 10. Khôi phục Bitmap cũ vào Memory DC
        SelectObject(hMemoryDC, hOldBitmap);
        // hOldBitmap is now selected back into the DC. It should not be deleted by us.

        // Các tài nguyên DC và Bitmap (hBitmapWrapper) sẽ tự động được giải phóng bởi các đối tượng RAII
        // GDI+ sẽ tự động được shutdown bởi GdiplusInitializer
    }
    catch (const std::exception &e)
    {
        // Ghi lỗi ra standard error
        std::wcerr << L"Lỗi trong CropScreen: " << e.what() << std::endl;
        // Consider logging instead of or in addition to MessageBox
        MessageBoxA(NULL, e.what(), "Lỗi Crop Ảnh", MB_ICONERROR | MB_OK);
    }
    catch (...)
    {
        // Bắt các loại ngoại lệ khác không xác định
        std::wcerr << L"Lỗi không xác định trong CropScreen." << std::endl;
        MessageBoxA(NULL, "Lỗi không xác định xảy ra khi crop ảnh.", "Lỗi Crop Ảnh", MB_ICONERROR | MB_OK);
    }

    // Nếu crop ảnh thành công, đọc văn bản và dịch
    if (success)
    {
        std::wstring extractedText;
        std::wstring translatedText;
        bool ocrOk = false;
        bool translateOk = false;

        // Bước OCR
        try
        {
            std::wcout << L"Đang đọc chữ từ ảnh..." << std::endl;
            extractedText = ReadTextFromImage(outputFilePath); // Gọi hàm OCR

            if (!extractedText.empty())
            {
                std::wcout << L"Văn bản được nhận dạng:\n======================\n"
                           << extractedText
                           << L"\n======================" << std::endl;
                ocrOk = true;
            }
            else
            {
                std::wcout << L"Không nhận dạng được văn bản hoặc có lỗi xảy ra trong quá trình OCR." << std::endl;
                MessageBoxW(NULL, L"Không nhận dạng được văn bản từ ảnh.", L"Lỗi OCR", MB_ICONWARNING | MB_OK);
            }
        }
        catch (const std::exception &ocr_ex)
        {
            std::wcerr << L"Lỗi trong quá trình OCR: " << ocr_ex.what() << std::endl;
            MessageBoxA(NULL, ocr_ex.what(), "Lỗi OCR", MB_ICONERROR | MB_OK);
        }
        catch (...)
        {
            std::wcerr << L"Lỗi không xác định trong quá trình OCR." << std::endl;
            MessageBoxA(NULL, "Lỗi không xác định xảy ra khi OCR.", "Lỗi OCR", MB_ICONERROR | MB_OK);
        }

        // Bước Dịch (chỉ thực hiện nếu OCR thành công và có văn bản)
        if (ocrOk)
        {
            try
            {
                std::wcout << L"Đang dịch văn bản (EN -> VI)..." << std::endl;
                // Gọi hàm dịch từ tiếng Anh ("en") sang tiếng Việt ("vi")
                translatedText = TranslateText(extractedText, "en", "vi");

                if (!translatedText.empty())
                {
                     // Kiểm tra xem có phải là thông báo lỗi placeholder không (tùy vào cách bạn triển khai TranslateText)
                    if (translatedText.find(L"Chức năng dịch chưa được cài đặt") == std::wstring::npos) {
                        std::wcout << L"Văn bản đã dịch:\n======================\n"
                                << translatedText
                                << L"\n======================" << std::endl;
                        translateOk = true;
                    } else {
                         std::wcout << L"Dịch thuật chưa được cấu hình." << std::endl;
                    }

                    // Hiển thị kết quả dịch (hoặc thông báo lỗi placeholder)
                    MessageBoxW(NULL, translatedText.c_str(), L"Kết quả dịch (EN -> VI)", MB_OK);
                }
                else
                {
                    std::wcout << L"Dịch thuật thất bại hoặc trả về chuỗi rỗng." << std::endl;
                    MessageBoxW(NULL, L"Không thể dịch văn bản.", L"Lỗi Dịch Thuật", MB_ICONWARNING | MB_OK);
                }
            }
            catch (const std::exception &trans_ex)
            {
                std::wcerr << L"Lỗi trong quá trình dịch: " << trans_ex.what() << std::endl;
                MessageBoxA(NULL, trans_ex.what(), "Lỗi Dịch Thuật", MB_ICONERROR | MB_OK);
            }
            catch (...)
            {
                std::wcerr << L"Lỗi không xác định trong quá trình dịch." << std::endl;
                MessageBoxA(NULL, "Lỗi không xác định xảy ra khi dịch.", "Lỗi Dịch Thuật", MB_ICONERROR | MB_OK);
            }
        }
    }
}