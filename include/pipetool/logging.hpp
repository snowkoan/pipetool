#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#include <windows.h>
#include <windows.h>

namespace pipetool::logging {

void log_message(std::wstring_view label, DWORD error_code);

void log_message(std::wstring_view label, DWORD error_code, std::span<const std::byte> payload);

std::wstring format_error(DWORD error_code);

void log_system_error(std::wstring_view label, const std::system_error& error);

} // namespace pipetool::logging
