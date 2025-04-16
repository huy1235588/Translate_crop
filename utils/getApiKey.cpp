#include "../include/getApiKey.h" // Bao gồm file header tương ứng
#include <fstream>                // Để làm việc với file (std::ifstream)
#include <string>                 // Để làm việc với chuỗi (std::string)
#include <stdexcept>              // Để ném ngoại lệ (std::runtime_error)
#include <algorithm>              // Cần cho std::find_if_not
#include <cctype>                 // Cần cho std::isspace (kiểm tra ký tự khoảng trắng)

namespace
{ // Sử dụng namespace ẩn danh để giới hạn phạm vi của các hàm trợ giúp

    // Hàm trợ giúp loại bỏ khoảng trắng ở đầu chuỗi (in-place)
    std::string &ltrim(std::string &s)
    {
        s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), [](unsigned char c)
                                            { return std::isspace(c); }));
        return s;
    }

    // Hàm trợ giúp loại bỏ khoảng trắng ở cuối chuỗi (in-place)
    std::string &rtrim(std::string &s)
    {
        s.erase(std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c)
                                 { return std::isspace(c); })
                    .base(),
                s.end());
        return s;
    }

    // Hàm trợ giúp loại bỏ khoảng trắng ở cả đầu và cuối chuỗi (in-place)
    std::string &trim(std::string &s)
    {
        return ltrim(rtrim(s));
    }

} // namespace

// Định nghĩa hàm đã được khai báo trong getApiKey.h
std::string getApiKeyFromConfig(const std::string &configPath)
{
    // Mở file cấu hình để đọc
    std::ifstream configFile(configPath);

    // Kiểm tra xem file có mở thành công không
    if (!configFile.is_open())
    {
        // Nếu không mở được, ném ngoại lệ với thông báo lỗi
        // Cân nhắc sử dụng std::system_error để có lỗi I/O cụ thể hơn nếu cần
        throw std::runtime_error("Không thể mở file cấu hình: " + configPath);
    }

    std::string line;                    // Biến để lưu từng dòng đọc được từ file
    const std::string keyToFind = "KEY"; // Tên khóa cần tìm trong file cấu hình

    // Đọc file từng dòng một
    while (std::getline(configFile, line))
    {
        // 1. Loại bỏ khoảng trắng ở đầu và cuối dòng đọc được
        trim(line);

        // 2. Bỏ qua các dòng trống hoặc dòng bắt đầu bằng ký tự comment ('#' hoặc ';')
        if (line.empty() || line[0] == '#' || line[0] == ';')
        {
            continue; // Chuyển sang dòng tiếp theo
        }

        // 3. Tìm vị trí của dấu '=' đầu tiên trong dòng
        size_t separatorPos = line.find('=');

        // 4. Kiểm tra xem dấu '=' có tồn tại và không phải là ký tự đầu tiên không
        if (separatorPos != std::string::npos && separatorPos > 0)
        {
            // Trích xuất phần tên khóa (trước dấu '=')
            std::string currentKey = line.substr(0, separatorPos);
            // Loại bỏ khoảng trắng thừa từ tên khóa
            trim(currentKey);

            // 5. So sánh tên khóa vừa trích xuất (đã trim) với khóa cần tìm (phân biệt hoa thường)
            if (currentKey == keyToFind)
            {
                // Trích xuất phần giá trị (sau dấu '=')
                std::string value = line.substr(separatorPos + 1);
                // Loại bỏ khoảng trắng thừa từ giá trị
                trim(value);

                // 6. Kiểm tra xem giá trị có rỗng không *sau khi* đã trim
                if (!value.empty())
                {
                    // Nếu tìm thấy khóa và giá trị không rỗng, trả về giá trị đó
                    // File sẽ tự động đóng khi configFile ra khỏi phạm vi (RAII)
                    return value;
                }
                else
                {
                    // Tìm thấy khóa nhưng giá trị lại rỗng sau khi trim
                    // Ném lỗi để thông báo tình trạng này
                    throw std::runtime_error("Tìm thấy khóa API ('" + keyToFind + "') nhưng giá trị rỗng trong file: " + configPath);
                }
            }
        }
        // Nếu dòng hiện tại không chứa khóa cần tìm, tiếp tục vòng lặp
    }

    // Nếu vòng lặp kết thúc mà không tìm thấy khóa (không return được)
    // Ném ngoại lệ thông báo không tìm thấy khóa
    // File sẽ tự động đóng khi configFile ra khỏi phạm vi (RAII)
    throw std::runtime_error("Không tìm thấy khóa API ('" + keyToFind + "=...') trong file cấu hình: " + configPath);
}