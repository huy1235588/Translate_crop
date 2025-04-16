#ifndef GET_API_KEY_H // Ngăn chặn việc include header nhiều lần
#define GET_API_KEY_H

#include <string> // Cần thiết cho std::string

/**
 * @brief Đọc API key từ một file cấu hình.
 *
 * Tìm kiếm trong file cấu hình được chỉ định một dòng có định dạng "KEY=YourApiKey".
 * Hàm sẽ loại bỏ khoảng trắng thừa ở đầu/cuối, bỏ qua các dòng trống
 * và các dòng bắt đầu bằng ký tự '#' hoặc ';'.
 *
 * @param configPath Đường dẫn đến file cấu hình. Mặc định là "config.ini".
 * @return Chuỗi API key nếu tìm thấy và không rỗng.
 * @throws std::runtime_error Nếu không thể mở file, không tìm thấy khóa,
 *                            hoặc khóa được tìm thấy nhưng giá trị của nó rỗng sau khi loại bỏ khoảng trắng.
 */
std::string getApiKeyFromConfig(const std::string& configPath = "config.ini");

#endif // GET_API_KEY_H