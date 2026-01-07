#pragma once

#include <cstddef>
#include <string>

namespace pipetool {

int fuzz_pipe(const std::wstring& pipe_name, std::size_t max_payload_size);

} // namespace pipetool
