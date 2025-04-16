#include "../include/CropScreen.h" // Xem xét đổi tên include nếu bạn đổi tên file header
#include "../include/ocr.h" // Bao gồm header cho chức năng OCR
#include "../include/translate.h" // Thêm include này
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
        { // Chỉ shutdown nếu khởi tạo thành công
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
    UINT size = 0; // Kích thước bộ nhớ cần thiết

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

// Hàm trợ giúp để lấy chuỗi lỗi của GDI+ (tùy chọn nhưng hữu ích)
std::string GdiplusStatusToString(Gdiplus::Status status) {
    switch (status) {
        case Gdiplus::Ok: return "Ok";
        case Gdiplus::GenericError: return "GenericError";
        case Gdiplus::InvalidParameter: return "InvalidParameter";
        case Gdiplus::OutOfMemory: return "OutOfMemory";
        case Gdiplus::ObjectBusy: return "ObjectBusy";
        case Gdiplus::InsufficientBuffer: return "InsufficientBuffer";
        case Gdiplus::NotImplemented: return "NotImplemented";
        case Gdiplus::Win32Error: return "Win32Error";
        case Gdiplus::WrongState: return "WrongState";
        case Gdiplus::Aborted: return "Aborted";
        case Gdiplus::FileNotFound: return "FileNotFound";
        case Gdiplus::ValueOverflow: return "ValueOverflow";
        case Gdiplus::AccessDenied: return "AccessDenied";
        case Gdiplus::UnknownImageFormat: return "UnknownImageFormat";
        case Gdiplus::FontFamilyNotFound: return "FontFamilyNotFound";
        case Gdiplus::FontStyleNotFound: return "FontStyleNotFound";
        case Gdiplus::NotTrueTypeFont: return "NotTrueTypeFont";
        case Gdiplus::UnsupportedGdiplusVersion: return "UnsupportedGdiplusVersion";
        case Gdiplus::GdiplusNotInitialized: return "GdiplusNotInitialized";
        case Gdiplus::PropertyNotFound: return "PropertyNotFound";
        case Gdiplus::PropertyNotSupported: return "PropertyNotSupported";
        #if (GDIPVER >= 0x0110)
        case Gdiplus::ProfileNotFound: return "ProfileNotFound";
        #endif
        default: return "UnknownStatus";
    }
}

// --- Chức năng chính ---

/**
 * @brief Chụp một vùng màn hình, lưu thành file PNG, thực hiện OCR và dịch.
 * @param ptStart Điểm bắt đầu.
 * @param ptEnd Điểm kết thúc.
 * @param outputFilePath Đường dẫn để lưu file ảnh PNG.
 * @return true nếu tất cả các bước (cắt, OCR, dịch) thành công, false nếu có lỗi.
 */
bool CropScreenAndProcess(POINT ptStart, POINT ptEnd, const std::wstring &outputFilePath) // Đổi tên cho rõ ràng
{
    // Khởi tạo GDI+
    GdiplusInitializer gdiplusInit;
    if (!gdiplusInit.IsInitialized())
    {
        std::wcerr << L"Lỗi: Không thể khởi tạo GDI+." << std::endl;
        MessageBoxW(NULL, L"Không thể khởi tạo GDI+.", L"Lỗi Hệ Thống", MB_ICONERROR | MB_OK);
        return false;
    }

    bool cropSuccess = false;
    HDC hScreenDC_raw = NULL; // Handle thô để sử dụng trong RAII
    HDC hMemoryDC_raw = NULL;
    HBITMAP hBitmap_raw = NULL;
    HBITMAP hOldBitmap = NULL; // Lưu lại bitmap ban đầu trong Memory DC

    try
    {
        // 1. Xác định tọa độ và kích thước vùng cắt
        int x = std::min(ptStart.x, ptEnd.x);
        int y = std::min(ptStart.y, ptEnd.y);
        int width = std::abs(ptEnd.x - ptStart.x);
        int height = std::abs(ptEnd.y - ptStart.y);

        if (width <= 0 || height <= 0)
        {
            throw std::runtime_error("Kích thước vùng cắt không hợp lệ.");
        }

        // 2. Lấy Screen DC
        DCWrapper screenDCWrapper((HWND)NULL);
        hScreenDC_raw = screenDCWrapper; // Lấy handle thô để sử dụng
        if (!screenDCWrapper.IsValid())
        {
            throw std::runtime_error("Không thể lấy DC màn hình.");
        }

        // 3. Tạo Memory DC
        DCWrapper memoryDCWrapper(hScreenDC_raw);
        hMemoryDC_raw = memoryDCWrapper; // Lấy handle thô
        if (!memoryDCWrapper.IsValid())
        {
            throw std::runtime_error("Không thể tạo Memory DC.");
        }

        // 4. Tạo Bitmap tương thích
        GdiObjectWrapper<HBITMAP> bitmapWrapper; // RAII cho bitmap mới
        hBitmap_raw = CreateCompatibleBitmap(hScreenDC_raw, width, height);
        if (!hBitmap_raw)
        {
            throw std::runtime_error("Không thể tạo Bitmap.");
        }
        bitmapWrapper = hBitmap_raw; // Gán cho wrapper để quản lý RAII

        // 5. Chọn Bitmap mới vào Memory DC
        hOldBitmap = (HBITMAP)SelectObject(hMemoryDC_raw, hBitmap_raw);
        if (!hOldBitmap || hOldBitmap == HGDI_ERROR) {
             throw std::runtime_error("Không thể chọn Bitmap vào Memory DC.");
        }

        // Cấu trúc RAII để khôi phục bitmap cũ
        struct OldBitmapRestorer {
            HDC dc; HBITMAP oldBmp; bool restore;
            OldBitmapRestorer(HDC hdc, HBITMAP hBmp) : dc(hdc), oldBmp(hBmp), restore(true) {}
            ~OldBitmapRestorer() { if (restore && dc && oldBmp) SelectObject(dc, oldBmp); }
        } oldBitmapRestorer(hMemoryDC_raw, hOldBitmap);


        // 6. Sao chép ảnh (BitBlt)
        if (!BitBlt(hMemoryDC_raw, 0, 0, width, height, hScreenDC_raw, x, y, SRCCOPY))
        {
            throw std::runtime_error("Lỗi sao chép hình ảnh (BitBlt).");
        }

        // 7. Tạo Gdiplus::Bitmap từ HBITMAP
        std::unique_ptr<Gdiplus::Bitmap> bitmap(Gdiplus::Bitmap::FromHBITMAP(hBitmap_raw, NULL));
        if (!bitmap)
        {
            // HBITMAP vẫn được quản lý bởi bitmapWrapper
            throw std::runtime_error("Không thể tạo Gdiplus::Bitmap từ HBITMAP.");
        }

        // 8. Lấy CLSID của bộ mã hóa PNG
        CLSID pngClsid;
        if (GetEncoderClsid(L"image/png", &pngClsid) < 0)
        {
            throw std::runtime_error("Không tìm thấy bộ mã hóa PNG.");
        }

        // 9. Lưu Bitmap thành file PNG
        Gdiplus::Status status = bitmap->Save(outputFilePath.c_str(), &pngClsid, NULL);
        if (status != Gdiplus::Ok)
        {
            std::string errorMsg = "Không thể lưu hình ảnh PNG. Lỗi GDI+: " + GdiplusStatusToString(status);
            throw std::runtime_error(errorMsg);
        }

        std::wcout << L"Đã lưu ảnh cắt vào: " << outputFilePath << std::endl;
        cropSuccess = true; // Cắt và lưu ảnh thành công

        // Bitmap cũ được khôi phục tự động bởi hàm hủy của oldBitmapRestorer
        // DC và Bitmap mới được giải phóng/xóa bởi các wrapper RAII tương ứng
        oldBitmapRestorer.restore = false; // Ngăn khôi phục lại nếu thoát bình thường
        SelectObject(hMemoryDC_raw, hOldBitmap); // Khôi phục ngay sau khi sử dụng

    }
    catch (const std::exception &e)
    {
        std::wcerr << L"Lỗi trong quá trình cắt ảnh: " << e.what() << std::endl;
        MessageBoxA(NULL, e.what(), "Lỗi Cắt Ảnh", MB_ICONERROR | MB_OK);
        // Đảm bảo dọn dẹp nếu phần nào đã thành công trước khi gặp lỗi
         if (hMemoryDC_raw && hOldBitmap) {
             SelectObject(hMemoryDC_raw, hOldBitmap); // Cố gắng khôi phục trong trường hợp lỗi
         }
        return false; // Báo lỗi
    }
    catch (...)
    {
        std::wcerr << L"Lỗi không xác định trong quá trình cắt ảnh." << std::endl;
        MessageBoxA(NULL, "Lỗi không xác định xảy ra khi cắt ảnh.", "Lỗi Cắt Ảnh", MB_ICONERROR | MB_OK);
         if (hMemoryDC_raw && hOldBitmap) {
             SelectObject(hMemoryDC_raw, hOldBitmap); // Cố gắng khôi phục trong trường hợp lỗi
         }
        return false; // Báo lỗi
    }

    // --- OCR và Dịch ---
    // Phần này chỉ thực hiện nếu cropSuccess là true. Xem xét đưa logic này ra ngoài hàm nếu cần.
    if (cropSuccess)
    {
        std::wstring extractedText;
        std::wstring translatedText;
        bool ocrOk = false;
        // bool translateOk = false; // Hiện không sử dụng

        // Bước OCR
        try
        {
            std::wcout << L"Đang đọc chữ từ ảnh..." << std::endl;
            extractedText = ReadTextFromImage(outputFilePath);

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
                MessageBoxW(NULL, L"Không nhận dạng được văn bản từ ảnh.", L"Thông báo OCR", MB_ICONWARNING | MB_OK);
                // Xem xét đây có phải lỗi nghiêm trọng hay không.
                // return false; // Có thể dừng nếu OCR thất bại
            }
        }
        catch (const std::exception &ocr_ex)
        {
            std::wcerr << L"Lỗi trong quá trình OCR: " << ocr_ex.what() << std::endl;
            MessageBoxA(NULL, ocr_ex.what(), "Lỗi OCR", MB_ICONERROR | MB_OK);
            return false; // Lỗi OCR được xem là nghiêm trọng
        }
        catch (...)
        {
            std::wcerr << L"Lỗi không xác định trong quá trình OCR." << std::endl;
            MessageBoxA(NULL, "Lỗi không xác định xảy ra khi OCR.", "Lỗi OCR", MB_ICONERROR | MB_OK);
            return false; // Lỗi OCR được xem là nghiêm trọng
        }

        // Bước Dịch
        if (ocrOk)
        {
            try
            {
                std::wcout << L"Đang dịch văn bản (EN -> VI)..." << std::endl;
                translatedText = TranslateText(extractedText, "en", "vi"); // Giả sử TranslateText xử lý lỗi nội bộ hoặc ném ngoại lệ

                if (!translatedText.empty())
                {
                     if (translatedText.find(L"Chức năng dịch chưa được cài đặt") == std::wstring::npos) {
                        std::wcout << L"Văn bản đã dịch:\n======================\n"
                                << translatedText
                                << L"\n======================" << std::endl;
                        // translateOk = true; // Thiết lập nếu cần ở nơi khác
                        MessageBoxW(NULL, translatedText.c_str(), L"Kết quả dịch (EN -> VI)", MB_OK);
                     } else {
                         std::wcout << L"Dịch thuật chưa được cấu hình hoặc trả về thông báo." << std::endl;
                         MessageBoxW(NULL, translatedText.c_str(), L"Thông báo Dịch Thuật", MB_ICONINFORMATION | MB_OK);
                         // Xem xét đây có phải lỗi hay không
                         // return false;
                     }
                }
                else
                {
                    std::wcout << L"Dịch thuật thất bại hoặc trả về chuỗi rỗng." << std::endl;
                    MessageBoxW(NULL, L"Không thể dịch văn bản.", L"Lỗi Dịch Thuật", MB_ICONWARNING | MB_OK);
                    // Xem xét đây có phải lỗi hay không
                    // return false;
                }
            }
            catch (const std::exception &trans_ex)
            {
                std::wcerr << L"Lỗi trong quá trình dịch: " << trans_ex.what() << std::endl;
                MessageBoxA(NULL, trans_ex.what(), "Lỗi Dịch Thuật", MB_ICONERROR | MB_OK);
                return false; // Lỗi dịch được xem là nghiêm trọng
            }
            catch (...)
            {
                std::wcerr << L"Lỗi không xác định trong quá trình dịch." << std::endl;
                MessageBoxA(NULL, "Lỗi không xác định xảy ra khi dịch.", "Lỗi Dịch Thuật", MB_ICONERROR | MB_OK);
                return false; // Lỗi dịch được xem là nghiêm trọng
            }
        }
         else {
             // OCR thất bại nhưng không ném ngoại lệ (ví dụ: không có văn bản)
             return false; // Hoặc true tuỳ mong muốn khi OCR không tìm thấy văn bản
         }
    } else {
        // Cắt ảnh thất bại
        return false;
    }

    return true; // Tất cả các bước thực hiện thành công
}