#include <iostream>
#include <filesystem>
#include <string>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <curl/curl.h>
#include "nlohmann/json.hpp" // Bao gồm header của thư viện JSON

#include "../include/translate.h"
#include "../include/string_utils.h"

// Sử dụng namespace cho thư viện JSON để tiện lợi
using json = nlohmann::json;

// Hàm callback để ghi dữ liệu nhận được vào một chuỗi
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

// Hàm phân tích cú pháp phản hồi JSON từ API sử dụng nlohmann/json
std::wstring ParseTranslationResponse(const std::string &response)
{
    std::string full_translation_utf8;
    try
    {
        // Phân tích chuỗi JSON
        json j = json::parse(response);

        // Kiểm tra cấu trúc JSON cơ bản (phải là một mảng)
        if (!j.is_array() || j.empty() || !j[0].is_array()) {
             throw std::runtime_error("Cấu trúc JSON không hợp lệ hoặc không chứa dữ liệu dịch.");
        }

        // j[0] là mảng chứa các đoạn dịch. Mỗi đoạn là một mảng con.
        // Ví dụ: j[0] = [ ["trans1", "orig1", ...], ["trans2", "orig2", ...], ... ]
        for (const auto& segment_array : j[0])
        {
            // Kiểm tra mỗi đoạn có phải là mảng và có phần tử đầu tiên (bản dịch) không
            if (segment_array.is_array() && !segment_array.empty() && segment_array[0].is_string())
            {
                // Lấy phần tử đầu tiên (chuỗi dịch) và nối vào kết quả
                full_translation_utf8 += segment_array[0].get<std::string>();
            }
             // Bỏ qua các phần tử không mong đợi trong mảng chính (ví dụ: null, "en", ...)
             // hoặc các đoạn không có cấu trúc đúng
        }

        if (full_translation_utf8.empty()) {
             // Trường hợp JSON hợp lệ nhưng không tìm thấy đoạn dịch nào
             throw std::runtime_error("Không tìm thấy văn bản dịch trong phản hồi JSON.");
        }

        return Utf8ToWstring(full_translation_utf8); // Chuyển đổi kết quả UTF-8 sang wstring
    }
    catch (json::parse_error &e)
    {
        std::cerr << "Lỗi phân tích JSON: " << e.what() << "\nPhản hồi: " << response << std::endl;
        return L"Lỗi phân tích phản hồi JSON";
    }
    catch (json::type_error &e) {
        std::cerr << "Lỗi truy cập kiểu JSON không hợp lệ: " << e.what() << "\nPhản hồi: " << response << std::endl;
        return L"Lỗi cấu trúc dữ liệu JSON";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Lỗi khi phân tích phản hồi dịch: " << e.what() << "\nPhản hồi: " << response << std::endl;
        return L"Lỗi khi phân tích phản hồi";
    }
}

// Hàm dịch văn bản sử dụng Google Translate API
std::wstring TranslateText(const std::wstring &inputText, const std::string &sourceLang, const std::string &targetLang)
{
    CURL *curl;
    CURLcode res;
    std::string readBuffer; // Chuỗi để lưu trữ phản hồi
    std::string utf8InputText;

    try
    {
        utf8InputText = WstringToUtf8(inputText); // Chuyển đổi wstring đầu vào sang UTF-8
    }
    catch (const std::exception &e)
    {
        std::cerr << "Lỗi chuyển đổi văn bản đầu vào sang UTF-8: " << e.what() << std::endl;
        return L"Lỗi mã hóa đầu vào: " + inputText;
    }

    // Chỉ gọi curl_global_init một lần khi bắt đầu chương trình
    // và curl_global_cleanup một lần khi kết thúc.
    // Tạm thời để ở đây để ví dụ hoạt động độc lập, nhưng nên chuyển ra ngoài.
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl)
    {
        // Mã hóa văn bản đầu vào theo định dạng URL
        char *encoded_text = curl_easy_escape(curl, utf8InputText.c_str(), utf8InputText.length());
        if (!encoded_text)
        {
            curl_easy_cleanup(curl);
            curl_global_cleanup(); // Nhớ cleanup global nếu init thất bại
            std::cerr << "Lỗi: curl_easy_escape() không thành công." << std::endl;
            return L"Lỗi mã hóa văn bản cho URL";
        }
        std::string encoded_text_str(encoded_text);
        curl_free(encoded_text); // Giải phóng chuỗi được trả về bởi curl_easy_escape

        // Xây dựng URL đầy đủ
        std::ostringstream url_stream;
        url_stream << "https://translate.googleapis.com/translate_a/single"
                   << "?client=gtx"
                   << "&sl=" << sourceLang
                   << "&tl=" << targetLang
                   // << "&hl=en-US" // Ngôn ngữ giao diện, thường không cần thiết cho API này
                   << "&dt=t"     // Yêu cầu dịch (dt=t là đủ)
                   // << "&dj=1" // Yêu cầu định dạng JSON đầy đủ hơn (có thể hữu ích)
                   << "&q=" << encoded_text_str;
        std::string url = url_stream.str();

        // Thiết lập các tùy chọn cho CURL
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback); // Hàm xử lý phản hồi
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);       // Bộ đệm lưu trữ phản hồi
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");     // Thiết lập user agent
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);           // Theo dõi chuyển hướng
        // Cân nhắc cấu hình xác minh SSL đúng cách thay vì vô hiệu hóa
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);           // Tạm thời vô hiệu hóa
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);           // Tạm thời vô hiệu hóa

        // Thực hiện yêu cầu
        res = curl_easy_perform(curl);

        // Kiểm tra lỗi
        if (res != CURLE_OK)
        {
            std::cerr << "curl_easy_perform() không thành công: " << curl_easy_strerror(res) << std::endl;
            // Không gọi curl_global_cleanup() ở đây nếu curl_easy_cleanup() được gọi
            curl_easy_cleanup(curl);
            // Nên gọi curl_global_cleanup() ở cuối chương trình
            return L"Lỗi yêu cầu API: " + Utf8ToWstring(curl_easy_strerror(res));
        }

        // Giải phóng tài nguyên CURL handle
        curl_easy_cleanup(curl);
    }
    else
    {
        // Không gọi curl_global_cleanup() ở đây nếu curl_easy_init() thất bại
        std::cerr << "Lỗi: khởi tạo curl_easy_init() không thành công." << std::endl;
        return L"Không khởi tạo được libcurl";
    }

    // Chỉ gọi curl_global_cleanup một lần khi kết thúc chương trình.
    // Tạm thời để ở đây để ví dụ hoạt động độc lập.
    curl_global_cleanup();

    // Phân tích phản hồi và trả về bản dịch
    return ParseTranslationResponse(readBuffer);
}