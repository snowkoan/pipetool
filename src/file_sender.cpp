#include "pipetool/file_sender.hpp"

#include "pipetool/logging.hpp"
#include "pipetool/pipe_client.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <system_error>
#include <vector>

#include <windows.h>

namespace pipetool {
namespace {

void log_error(const std::wstring& message, DWORD error) {
    logging::log_message(message, error);
}

} // namespace

int stream_file(const std::wstring& pipe_name, const std::filesystem::path& file_path) {
    try {
        std::ifstream input {file_path, std::ios::binary};
        if (!input) {
            std::wcerr << L"Unable to open file: " << file_path.wstring() << L"\n";
            return EXIT_FAILURE;
        }

        PipeClient pipe = PipeClient::connect(pipe_name, GENERIC_WRITE | GENERIC_READ, 0, FILE_ATTRIBUTE_NORMAL);

        input.seekg(0, std::ios::end);
        const std::streamsize file_size = input.tellg();
        if (file_size < 0) {
            std::wcerr << L"Unable to determine file size: " << file_path.wstring() << L"\n";
            return EXIT_FAILURE;
        }
        input.seekg(0, std::ios::beg);

        std::vector<std::byte> buffer(static_cast<std::size_t>(file_size));
        if (!input.read(reinterpret_cast<char*>(buffer.data()), file_size)) {
            std::wcerr << L"Error while reading file: " << file_path.wstring() << L"\n";
            return EXIT_FAILURE;
        }

        pipe.write(std::span<const std::byte>(buffer.data(), buffer.size()));

        if (!::FlushFileBuffers(pipe.native_handle())) {
            const DWORD error = ::GetLastError();
            log_error(L"FlushFileBuffers", error);
        }

        std::array<std::byte, 4096> response_buffer {};
        while (true) {
            const auto result = pipe.read(std::span<std::byte>(response_buffer.data(), response_buffer.size()));
            if (result.error == ERROR_SUCCESS) {
                if (result.bytes_transferred == 0) {
                    break;
                }
                logging::log_message(L"Pipe response", ERROR_SUCCESS, std::span<const std::byte>(response_buffer.data(), result.bytes_transferred));
                continue;
            }

            if (result.error == ERROR_MORE_DATA) {
                logging::log_message(L"Pipe response", ERROR_MORE_DATA, std::span<const std::byte>(response_buffer.data(), result.bytes_transferred));
                continue;
            }

            if (result.error == ERROR_BROKEN_PIPE || result.error == ERROR_PIPE_NOT_CONNECTED) {
                log_error(L"Pipe connection closed", result.error);
                break;
            }

            log_error(L"Pipe read error", result.error);
            return static_cast<int>(result.error);
        }

        return EXIT_SUCCESS;
    } catch (const std::system_error& ex) {
        logging::log_system_error(L"Stream failed", ex);
        return EXIT_FAILURE;
    } catch (const std::exception& ex) {
        std::cerr << "Stream failed: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}

} // namespace pipetool
