#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <windows.h>

#include "pipetool/file_sender.hpp"
#include "pipetool/logging.hpp"
#include "pipetool/pipe_info.hpp"
#include "pipetool/random_sender.hpp"

namespace {

constexpr std::size_t kDefaultFuzzSize = 100;

[[nodiscard]] int print_usage() {
    std::wcerr << L"Usage: pipetool <pipename> <subcommand> [options]\n\n"
               << L"Subcommands:\n"
               << L"  --stream-file <path>   Stream the entire file into the pipe.\n"
               << L"  --fuzz [bytes]         Send random payloads (default 100 bytes).\n"
               << L"  --info                 Display security-related pipe metadata.\n";
    return EXIT_FAILURE;
}

[[nodiscard]] std::size_t parse_size(const std::wstring& param) {
    try {
        std::size_t processed = 0;
        unsigned long long value = std::stoull(param, &processed, 10);
        if (processed != param.size()) {
            throw std::invalid_argument("trailing characters");
        }
        if (value == 0) {
            throw std::invalid_argument("zero payload is invalid");
        }
        if (value > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
            throw std::out_of_range("size overflow");
        }
        return static_cast<std::size_t>(value);
    } catch (const std::exception&) {
        throw std::invalid_argument("invalid payload size parameter");
    }
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        if (argc < 3) {
            return print_usage();
        }

        std::wstring pipe_name = argv[1];
        std::wstring subcommand = argv[2];

        if (subcommand == L"--stream-file") {
            if (argc != 4) {
                std::wcerr << L"--stream-file requires a file path argument.\n";
                return print_usage();
            }
            std::filesystem::path file_path {argv[3]};
            if (!std::filesystem::exists(file_path)) {
                std::wcerr << L"File not found: " << file_path.wstring() << L"\n";
                return EXIT_FAILURE;
            }
            return pipetool::stream_file(pipe_name, file_path);
        }

        if (subcommand == L"--fuzz") {
            if (argc > 4) {
                std::wcerr << L"--fuzz accepts at most one size argument.\n";
                return print_usage();
            }
            std::size_t payload_size = kDefaultFuzzSize;
            if (argc >= 4) {
                payload_size = parse_size(argv[3]);
            }
            return pipetool::fuzz_pipe(pipe_name, payload_size);
        }

        if (subcommand == L"--info") {
            if (argc != 3) {
                std::wcerr << L"--info does not accept additional arguments.\n";
                return print_usage();
            }
            return pipetool::show_pipe_info(pipe_name);
        }

        std::wcerr << L"Unknown subcommand: " << subcommand << L"\n";
        return print_usage();
    } catch (const std::system_error& ex) {
        pipetool::logging::log_system_error(L"Fatal error", ex);
        return EXIT_FAILURE;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
