#include "pipetool/logging.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <windows.h>

namespace pipetool::logging {
namespace {

class ConsoleColorScope {
public:
    explicit ConsoleColorScope(bool success) {
        handle_ = ::GetStdHandle(STD_OUTPUT_HANDLE);
        if (handle_ == INVALID_HANDLE_VALUE || handle_ == nullptr) {
            handle_ = nullptr;
            return;
        }

        CONSOLE_SCREEN_BUFFER_INFO info {};
        if (!::GetConsoleScreenBufferInfo(handle_, &info)) {
            handle_ = nullptr;
            return;
        }

        original_attributes_ = info.wAttributes;
        const WORD desired = success ? (FOREGROUND_GREEN | FOREGROUND_INTENSITY) : (FOREGROUND_RED | FOREGROUND_INTENSITY);
        if (!::SetConsoleTextAttribute(handle_, desired)) {
            handle_ = nullptr;
        }
    }

    ~ConsoleColorScope() {
        if (handle_ != nullptr) {
            ::SetConsoleTextAttribute(handle_, original_attributes_);
        }
    }

private:
    HANDLE handle_ {nullptr};
    WORD original_attributes_ {0};
};

std::wstring sanitize_message(std::wstring_view message) {
    std::wstring clean {message};
    for (auto& ch : clean) {
        if (ch == L'\r' || ch == L'\n') {
            ch = L' ';
        }
    }
    return clean;
}

void write_hex_dump(std::span<const std::byte> payload) {
    constexpr std::size_t kRowWidth = 16;
    const std::size_t size = payload.size();

    for (std::size_t offset = 0; offset < size; offset += kRowWidth) {
        std::wcout << L"    " << std::setw(6) << std::setfill(L'0') << std::hex << offset << L"  ";

        std::wcout << std::dec << std::setfill(L' ');
        for (std::size_t column = 0; column < kRowWidth; ++column) {
            const std::size_t index = offset + column;
            if (index < size) {
                const auto byte_value = static_cast<unsigned char>(payload[index]);
                std::wcout << std::setw(2) << std::setfill(L'0') << std::hex << static_cast<unsigned int>(byte_value) << L' ';
                std::wcout << std::dec;
            } else {
                std::wcout << L"   ";
            }
        }

        std::wcout << L" |";
        for (std::size_t column = 0; column < kRowWidth; ++column) {
            const std::size_t index = offset + column;
            if (index < size) {
                const auto byte_value = static_cast<unsigned char>(payload[index]);
                const bool printable = byte_value >= 32 && byte_value <= 126;
                std::wcout << (printable ? static_cast<wchar_t>(byte_value) : L'.');
            } else {
                std::wcout << L' ';
            }
        }
        std::wcout << L"|\n";
    }

    if (size == 0) {
        std::wcout << L"    <empty>\n";
    }

    std::wcout << std::dec << std::setfill(L' ');
}

void write_log(std::wstring_view label, DWORD error_code, std::span<const std::byte> payload, bool include_payload) {
    const ConsoleColorScope scope {error_code == ERROR_SUCCESS};
    const std::wstring message = format_error(error_code);

    std::wcout << L"[" << error_code << L"] " << label;
    if (!message.empty()) {
        std::wcout << L" - " << message;
    }
    std::wcout << L"\n";

    if (include_payload) {
        write_hex_dump(payload);
    }
}

} // namespace

void log_message(std::wstring_view label, DWORD error_code) {
    write_log(label, error_code, {}, false);
}

void log_message(std::wstring_view label, DWORD error_code, std::span<const std::byte> payload) {
    write_log(label, error_code, payload, true);
}

std::wstring format_error(DWORD error_code) {
    if (error_code == ERROR_SUCCESS) {
        return L"OK";
    }

    LPWSTR buffer = nullptr;
    const DWORD length = ::FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        0,
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);

    if (length == 0 || buffer == nullptr) {
        return L"Unknown error";
    }

    std::wstring message {buffer, buffer + length};
    ::LocalFree(buffer);
    return sanitize_message(message);
}

void log_system_error(std::wstring_view label, const std::system_error& error) {
    const DWORD code = static_cast<DWORD>(error.code().value());
    std::wstring composed {label};

    const std::string what = error.what();
    if (!what.empty()) {
        const std::string_view what_view {what};
        const std::size_t pos = what_view.find(':');
        const std::size_t limit = (pos == std::string_view::npos) ? what_view.size() : pos;

        composed.append(L" [");
        for (std::size_t i = 0; i < limit; ++i) {
            composed.push_back(static_cast<wchar_t>(what_view[i]));
        }
        composed.append(L"]");
    }

    log_message(composed, code);
}

} // namespace pipetool::logging
