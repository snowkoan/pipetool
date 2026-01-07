#include "pipetool/random_sender.hpp"

#include "pipetool/logging.hpp"
#include "pipetool/pipe_client.hpp"

#include <algorithm>
#include <cstddef>
#include <chrono>
#include <conio.h>
#include <iostream>
#include <random>
#include <span>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

#include <windows.h>

namespace pipetool {
namespace {

void log_error(const std::wstring& label, DWORD error) {
    logging::log_message(label, error);
}

PipeClient connect_pipe_with_retry(const std::wstring& pipe_name) {
    while (true) {
        try {
            return PipeClient::connect(pipe_name, GENERIC_WRITE | GENERIC_READ, 0, FILE_ATTRIBUTE_NORMAL);
        } catch (const std::system_error& ex) {
            logging::log_system_error(L"Pipe connect failed, retrying", ex);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

bool emit_available_responses(PipeClient& pipe, std::vector<std::byte>& buffer, bool& connection_closed) {
    connection_closed = false;

    DWORD available = 0;
    if (!::PeekNamedPipe(pipe.native_handle(), nullptr, 0, nullptr, &available, nullptr)) {
        const DWORD error = ::GetLastError();
        if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
            log_error(L"Pipe connection closed", error);
            connection_closed = true;
            return false;
        }
        log_error(L"PeekNamedPipe", error);
        return false;
    }

    while (available > 0) {
        const std::size_t chunk = std::min<std::size_t>(buffer.size(), static_cast<std::size_t>(available));
        auto result = pipe.read(std::span<std::byte>{buffer.data(), chunk});

        const std::size_t bytes = static_cast<std::size_t>(result.bytes_transferred);
        logging::log_message(L"Pipe response", result.error, std::span<const std::byte>{buffer.data(), bytes});

        if (result.error == ERROR_BROKEN_PIPE || result.error == ERROR_PIPE_NOT_CONNECTED) {
            log_error(L"Pipe connection closed", result.error);
            connection_closed = true;
            return false;
        }
        if (result.error != ERROR_SUCCESS && result.error != ERROR_MORE_DATA) {
            log_error(L"Pipe read error", result.error);
            return false;
        }
        if (result.error == ERROR_SUCCESS && bytes == 0) {
            break;
        }

        available = (available > result.bytes_transferred) ? available - result.bytes_transferred : 0;
        if (available == 0) {
            if (!::PeekNamedPipe(pipe.native_handle(), nullptr, 0, nullptr, &available, nullptr)) {
                const DWORD error = ::GetLastError();
                if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
                    log_error(L"Pipe connection closed", error);
                    connection_closed = true;
                    return false;
                }
                log_error(L"PeekNamedPipe", error);
                return false;
            }
        }
    }

    return true;
}

} // namespace

int fuzz_pipe(const std::wstring& pipe_name, std::size_t max_payload_size) {
    if (max_payload_size == 0) {
        std::wcerr << L"Max payload size must be greater than zero.\n";
        return EXIT_FAILURE;
    }

    try {
        PipeClient pipe = connect_pipe_with_retry(pipe_name);

        std::mt19937 rng(static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        std::uniform_int_distribution<std::size_t> size_dist(1, max_payload_size);
        std::uniform_int_distribution<int> byte_dist(0, 255);

        std::vector<std::byte> payload(max_payload_size);
        std::vector<std::byte> response(4096);

        logging::log_message(L"Fuzzing started", ERROR_SUCCESS);

        while (true) {
            if (_kbhit()) {
                _getch();
                logging::log_message(L"User requested stop", ERROR_SUCCESS);
                break;
            }

            const std::size_t payload_size = size_dist(rng);
            for (std::size_t i = 0; i < payload_size; ++i) {
                payload[i] = static_cast<std::byte>(byte_dist(rng));
            }

            logging::log_message(L"Payload", ERROR_SUCCESS, std::span<const std::byte>{payload.data(), payload_size});

            bool write_complete = false;
            while (!write_complete) {
                try {
                    pipe.write(std::span<const std::byte>{payload.data(), payload_size});
                    write_complete = true;
                } catch (const std::system_error& ex) {
                    const DWORD code = static_cast<DWORD>(ex.code().value());
                    if (code == ERROR_BROKEN_PIPE || code == ERROR_PIPE_NOT_CONNECTED || code == ERROR_NO_DATA) {
                        logging::log_system_error(L"Pipe write failed, reconnecting", ex);
                        pipe = connect_pipe_with_retry(pipe_name);
                        continue;
                    }
                    throw;
                }
            }

            bool connection_closed = false;
            if (!emit_available_responses(pipe, response, connection_closed)) {
                if (connection_closed) {
                    pipe = connect_pipe_with_retry(pipe_name);
                    continue;
                }
                return EXIT_FAILURE;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return EXIT_SUCCESS;
    } catch (const std::system_error& ex) {
        logging::log_system_error(L"Fuzzing failed", ex);
        return EXIT_FAILURE;
    } catch (const std::exception& ex) {
        std::cerr << "Fuzzing failed: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}

} // namespace pipetool
