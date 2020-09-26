#pragma once


struct ConversionException { std::string str; };

std::string convertToUTF8(std::wstring_view wideString, bool failOnInvalidCharacter = true);

std::wstring convertToUTF16(std::string_view narrowString, bool failOnInvalidCharacter = true);


struct FileSystemException { std::string str; };

std::wstring createTmpCopy(const wchar_t* file, const wchar_t* prefix);

void deleteFile(const wchar_t* file);
