#include <iostream>
#include <windows.h>
#include "../include/OverlayWindow.h"    // Bao gồm header cho cửa sổ overlay
#include "../include/CropScreen.h" // Bao gồm header cho chức năng crop và dịch (nếu cần gọi từ đây)
// #include "../include/ocr.h"              // Bao gồm header cho chức năng OCR (nếu cần gọi từ đây)

// Đường dẫn đến file ảnh đã crop
const std::wstring CROPPED_IMAGE_FILENAME = L"captured_region.png";

// Hàm callback được gọi khi hotkey được nhấn
void OnHotkeyPressed()
{
    // Tạo và hiển thị cửa sổ overlay để chọn vùng màn hình
    // Hàm này sẽ chặn cho đến khi người dùng chọn xong hoặc hủy bỏ
    CreateOverlayWindow(CROPPED_IMAGE_FILENAME);

    // Lấy tọa độ đã chọn (nếu cần xử lý ở đây thay vì trong OverlayWindow)
    // POINT start, end;
    // GetSelectionPoints(&start, &end); // Giả sử có hàm này trong OverlayWindow.h/cpp
    // CropAndTranslate(start, end); // Gọi hàm crop và xử lý ảnh
}

int main()
{
    // Cấu hình console hỗ trợ Unicode
    SetConsoleOutputCP(CP_UTF8); // Đặt mã hóa đầu ra console thành UTF-8
    SetConsoleCP(CP_UTF8);       // Đặt mã hóa đầu vào console thành UTF-8

    // Đăng ký hotkey toàn cục: Ctrl + Alt + D (0x44 là mã VK cho phím 'D')
    if (!RegisterHotKey(
            NULL,                  // Sử dụng handle của luồng hiện tại
            1,                     // ID duy nhất cho hotkey này
            MOD_CONTROL | MOD_ALT, // Tổ hợp phím Ctrl + Alt
            0x44))                 // Mã phím ảo cho 'D'
    {
        // Ghi lỗi ra console nếu đăng ký thất bại
        std::cerr << "Không thể đăng ký hotkey." << std::endl;
        return 1; // Thoát chương trình với mã lỗi
    }

    std::cout << "Hotkey Ctrl + Alt + D đã được đăng ký. Nhấn tổ hợp phím để chụp màn hình." << std::endl;

    // Vòng lặp xử lý thông điệp chính của ứng dụng
    MSG msg = {0};
    // Lấy thông điệp từ hàng đợi của luồng
    // GetMessage sẽ chặn cho đến khi có thông điệp (bao gồm cả WM_HOTKEY hoặc WM_QUIT)
    while (GetMessage(&msg, NULL, 0, 0) > 0) // > 0 để xử lý lỗi GetMessage trả về -1
    {
        // Kiểm tra xem thông điệp có phải là WM_HOTKEY không
        if (msg.message == WM_HOTKEY)
        {
            // Kiểm tra ID của hotkey (đảm bảo đúng là hotkey chúng ta đã đăng ký)
            if (msg.wParam == 1) // ID = 1 mà chúng ta đã đăng ký
            {
                std::cout << "Hotkey Ctrl + Alt + D được nhấn!" << std::endl;
                // Gọi hàm xử lý khi hotkey được nhấn
                OnHotkeyPressed();
            }
        }
        else
        {
            // Dịch các thông điệp bàn phím ảo thành thông điệp ký tự (nếu cần)
            TranslateMessage(&msg);
            // Gửi thông điệp đến thủ tục cửa sổ thích hợp
            DispatchMessage(&msg);
        }
    }

    // Hủy đăng ký hotkey trước khi thoát ứng dụng để giải phóng tài nguyên
    UnregisterHotKey(NULL, 1); // Sử dụng cùng handle và ID đã đăng ký

    return 0; // Kết thúc chương trình thành công
}