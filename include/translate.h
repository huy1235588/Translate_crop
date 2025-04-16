#ifndef TRANSLATE_H
#define TRANSLATE_H

#include <string>

/**
 * @brief Dịch văn bản từ ngôn ngữ nguồn sang ngôn ngữ đích.
 *
 * @param inputText Văn bản cần dịch (định dạng wstring).
 * @param sourceLang Mã ngôn ngữ nguồn (ví dụ: "en").
 * @param targetLang Mã ngôn ngữ đích (ví dụ: "vi").
 * @return std::wstring Văn bản đã được dịch, hoặc chuỗi rỗng nếu có lỗi.
 *
 * @note Đây là một hàm giả định. Bạn cần triển khai logic gọi API
 *       hoặc thư viện dịch thuật thực tế ở đây.
 */
std::wstring TranslateText(const std::wstring& inputText, const std::string& sourceLang, const std::string& targetLang);

#endif // TRANSLATE_H