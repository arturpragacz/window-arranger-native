#include "pch.h"
#include "windows_utils.h"

using std::to_string;
#define EXCEPTION_STRING std::string(__func__)


std::string convertToUTF8(std::wstring_view wideString, bool failOnInvalidCharacter) {
	std::string narrowString;
	if (wideString.empty())
		return narrowString;

	int oldLength = static_cast<int>(wideString.length());
	int bufferSize = oldLength * 3;
	char* buffer = new char[bufferSize];

	DWORD flags = 0;
	if (failOnInvalidCharacter)
		flags = WC_ERR_INVALID_CHARS;
	int newLength = WideCharToMultiByte(CP_UTF8, flags, wideString.data(), oldLength, buffer, bufferSize, NULL, NULL);
	if (!newLength)
		throw UTFConversionException{ EXCEPTION_STRING + ": " + to_string(GetLastError()) };

	narrowString.assign(buffer, newLength);
	delete[] buffer;

	return narrowString;
}

std::wstring convertToUTF16(std::string_view narrowString, bool failOnInvalidCharacter) {
	std::wstring wideString;
	if (narrowString.empty())
		return wideString;

	int oldLength = static_cast<int>(narrowString.length());
	int bufferSize = oldLength;
	wchar_t* buffer = new wchar_t[bufferSize];

	DWORD flags = 0;
	if (failOnInvalidCharacter)
		flags =	MB_ERR_INVALID_CHARS;
	int newLength = MultiByteToWideChar(CP_UTF8, flags, narrowString.data(), oldLength, buffer, bufferSize);
	if (!newLength)
		throw UTFConversionException{ EXCEPTION_STRING + ": " + to_string(GetLastError()) };

	wideString.assign(buffer, newLength);
	delete[] buffer;

	return wideString;
}


std::wstring createTmpCopy(const wchar_t* file, const wchar_t* prefix) {
	wchar_t tmpPath[MAX_PATH];
	wchar_t target[MAX_PATH];

	auto tmpPathLength = GetTempPath(MAX_PATH, tmpPath);
	if (tmpPathLength > MAX_PATH || (tmpPathLength == 0))
		throw FileSystemException{ EXCEPTION_STRING + ": can't access tmp path, error: " + to_string(GetLastError()) };

	if (!GetTempFileName(tmpPath, prefix, 0, target))
		throw FileSystemException{ EXCEPTION_STRING + ": can't create tmp file, error: " + to_string(GetLastError()) };

	if (!CopyFile(file, target, FALSE))
		throw FileSystemException{ EXCEPTION_STRING + ": can't copy [" + convertToUTF8(file, false) + "] to [" + convertToUTF8(target, false) + "], error: " + to_string(GetLastError()) };

	return target;
}

void deleteFile(const wchar_t* file) {
	if (!DeleteFile(file))
		throw FileSystemException{ EXCEPTION_STRING + ": [" + convertToUTF8(file, false) + "], error: " + to_string(GetLastError()) };
}
