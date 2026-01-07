#include "pipetool/pipe_client.hpp"

#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include <windows.h>

namespace pipetool {
namespace {

bool has_prefix(std::wstring_view value, std::wstring_view prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::wstring normalize_pipe_name(const std::wstring& pipe_name) {
    constexpr std::wstring_view kPrefix = L"\\\\.\\pipe\\";
    if (has_prefix(pipe_name, kPrefix)) {
        return pipe_name;
    }
    std::wstring qualified {kPrefix};
    qualified.append(pipe_name);
    return qualified;
}

[[noreturn]] void throw_last_error(std::string_view context) {
    const DWORD error = ::GetLastError();
    throw std::system_error(static_cast<int>(error), std::system_category(), std::string(context));
}

} // namespace

PipeClient::PipeClient(PipeClient&& other) noexcept {
    *this = std::move(other);
}

PipeClient& PipeClient::operator=(PipeClient&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        full_name_ = std::move(other.full_name_);
        other.handle_ = INVALID_HANDLE_VALUE;
    }
    return *this;
}

PipeClient::~PipeClient() {
    close();
}

PipeClient PipeClient::connect(const std::wstring& pipe_name, DWORD desired_access, DWORD share_mode, DWORD flags_and_attributes) {
    const std::wstring qualified = normalize_pipe_name(pipe_name);

    if (!::WaitNamedPipeW(qualified.c_str(), 5000)) {
        throw_last_error("WaitNamedPipeW");
    }

    HANDLE handle = ::CreateFileW(
        qualified.c_str(),
        desired_access,
        share_mode,
        nullptr,
        OPEN_EXISTING,
        flags_and_attributes,
        nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        throw_last_error("CreateFileW");
    }

    return PipeClient {handle, qualified};
}

bool PipeClient::is_valid() const noexcept {
    return handle_ != INVALID_HANDLE_VALUE;
}

HANDLE PipeClient::native_handle() const noexcept {
    return handle_;
}

std::wstring PipeClient::qualified_name() const {
    return full_name_;
}

void PipeClient::write(std::span<const std::byte> buffer) const {
    if (!is_valid()) {
        throw std::runtime_error("Pipe handle is not valid");
    }

    const std::byte* data = buffer.data();
    std::size_t remaining = buffer.size();

    while (remaining > 0) {
        const DWORD chunk = remaining > static_cast<std::size_t>(std::numeric_limits<DWORD>::max())
            ? std::numeric_limits<DWORD>::max()
            : static_cast<DWORD>(remaining);

        DWORD written = 0;
        if (!::WriteFile(handle_, reinterpret_cast<LPCVOID>(data), chunk, &written, nullptr)) {
               throw_last_error("WriteFile");
        }

        if (written == 0) {
            throw std::runtime_error("WriteFile wrote zero bytes");
        }

        data += written;
        remaining -= written;
    }
}

PipeClient::ReadResult PipeClient::read(std::span<std::byte> buffer) const {
    if (!is_valid()) {
        throw std::runtime_error("Pipe handle is not valid");
    }

    if (buffer.empty()) {
        return {0, ERROR_SUCCESS};
    }

    DWORD read = 0;
    if (!::ReadFile(handle_, reinterpret_cast<LPVOID>(buffer.data()), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
        const DWORD error = ::GetLastError();
            return {read, error};
    }

    return {read, ERROR_SUCCESS};
}

PipeClient::PipeClient(HANDLE handle, std::wstring full_name) noexcept
    : handle_(handle), full_name_(std::move(full_name)) {}

void PipeClient::close() noexcept {
    if (handle_ != INVALID_HANDLE_VALUE) {
        ::CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
}

} // namespace pipetool
