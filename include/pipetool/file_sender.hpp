#pragma once

#include <filesystem>
#include <string>

namespace pipetool {

int stream_file(const std::wstring& pipe_name, const std::filesystem::path& file_path);

} // namespace pipetool
