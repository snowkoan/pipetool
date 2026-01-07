#include <utility>
#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <windows.h>

namespace pipetool {

class PipeClient {
public:
    PipeClient() noexcept = default;
    PipeClient(const PipeClient&) = delete;
    PipeClient& operator=(const PipeClient&) = delete;
    PipeClient(PipeClient&& other) noexcept;
    PipeClient& operator=(PipeClient&& other) noexcept;
    ~PipeClient();

    static PipeClient connect(const std::wstring& pipe_name, DWORD desired_access, DWORD share_mode, DWORD flags_and_attributes);

    bool is_valid() const noexcept;

    HANDLE native_handle() const noexcept;

    std::wstring qualified_name() const;

    void write(std::span<const std::byte> buffer) const;

    struct ReadResult {
        DWORD bytes_transferred;
        DWORD error;
    };

    ReadResult read(std::span<std::byte> buffer) const;

private:
    explicit PipeClient(HANDLE handle, std::wstring full_name) noexcept;

    void close() noexcept;

    HANDLE handle_ {INVALID_HANDLE_VALUE};
    std::wstring full_name_;
};

} // namespace pipetool
