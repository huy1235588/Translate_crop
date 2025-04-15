# Translate Crop

## Mô tả

Đây là một ứng dụng Windows cho phép người dùng chụp một vùng màn hình được chọn bằng phím tắt (`Ctrl + Alt + D`), thực hiện Nhận dạng ký tự quang học (OCR) trên ảnh đã chụp bằng Tesseract và hiển thị văn bản được nhận dạng.

## Tính năng

*   Phím tắt toàn cục (`Ctrl + Alt + D`) để bắt đầu chụp màn hình.
*   Lớp phủ trong suốt để chọn vùng màn hình.
*   Chụp vùng đã chọn và lưu dưới dạng `captured_region.png`.
*   Thực hiện OCR trên ảnh đã chụp bằng Tesseract (hỗ trợ tiếng Việt và tiếng Anh).
*   Hiển thị văn bản được nhận dạng trong hộp thoại và in ra console.

## Dependencies (Thư viện phụ thuộc)

*   Hệ điều hành Windows
*   Trình biên dịch C++ tương thích C++17 (ví dụ: MinGW-w64 GCC)
*   CMake (phiên bản 3.15 trở lên)
*   Hệ thống xây dựng Ninja (hoặc Make)
*   Thư viện Tesseract OCR (bao gồm header và thư viện phát triển)
*   Thư viện Leptonica (bao gồm header và thư viện phát triển)
*   Dữ liệu ngôn ngữ Tesseract (`tessdata`), cụ thể là tiếng Anh (`eng`) và tiếng Việt (`vie`). Đường dẫn hiện được đặt cứng trong [`src/ocr.cpp`](src/ocr.cpp) là `D:\\Program Files\\MSYS2\\mingw64\\share\\tessdata`. Bạn có thể cần điều chỉnh đường dẫn này.

## Building the Project (Xây dựng dự án)

1.  **Cài đặt Dependencies:** Đảm bảo tất cả các thư viện phụ thuộc đã được cài đặt. Ví dụ, sử dụng MSYS2:
    ```bash
    pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-tesseract-ocr mingw-w64-x86_64-leptonica mingw-w64-x86_64-tesseract-ocr-data-eng mingw-w64-x86_64-tesseract-ocr-data-vie
    ```
2.  **Kiểm tra đường dẫn `tessdata`:** Nếu cần, hãy cập nhật biến `TESSDATA_PATH` trong [`src/ocr.cpp`](src/ocr.cpp) để trỏ đến thư mục `tessdata` trên hệ thống của bạn.
3.  **Tạo thư mục build:**
    ```bash
    mkdir build
    cd build
    ```
4.  **Cấu hình CMake:** Sử dụng Ninja làm trình tạo (generator):
    ```bash
    cmake .. -G Ninja
    ```
    Hoặc sử dụng trình tạo mặc định (thường là Make):
    ```bash
    cmake ..
    ```
5.  **Biên dịch dự án:** Nếu sử dụng Ninja:
    ```bash
    ninja
    ```
    Nếu sử dụng Make:
    ```bash
    make
    ```
6.  File thực thi (`TranslateCrop.exe`) sẽ được tạo trong thư mục `build`.

## Usage (Sử dụng)

1.  Chạy file thực thi `TranslateCrop.exe` từ thư mục `build`.
2.  Nhấn tổ hợp phím `Ctrl + Alt + D`.
3.  Một lớp phủ sẽ xuất hiện. Nhấp và kéo chuột để chọn một vùng trên màn hình.
4.  Thả nút chuột.
5.  Vùng đã chọn sẽ được lưu dưới dạng `captured_region.png` trong thư mục chứa file thực thi.
6.  OCR sẽ được thực hiện và văn bản được nhận dạng sẽ hiển thị trong hộp thoại và được in ra console.
7.  Nhấn phím `Esc` trong khi chọn để hủy bỏ.

## Project Structure (Cấu trúc dự án)
