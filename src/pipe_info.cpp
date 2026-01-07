#include "pipetool/pipe_info.hpp"

#include "pipetool/logging.hpp"
#include "pipetool/pipe_client.hpp"

#include <aclapi.h>
#include <sddl.h>

#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <system_error>

#include <windows.h>

namespace pipetool {
namespace {

std::wstring describe_pipe_type(DWORD flags) {
    if ((flags & PIPE_TYPE_MESSAGE) != 0) {
        return L"Message";
    }
    if ((flags & PIPE_TYPE_BYTE) != 0) {
        return L"Byte";
    }
    return L"Unknown";
}

std::wstring describe_read_mode(DWORD state) {
    if ((state & PIPE_READMODE_MESSAGE) != 0) {
        return L"Message";
    }
    if ((state & PIPE_READMODE_BYTE) != 0) {
        return L"Byte";
    }
    return L"Unknown";
}

std::wstring describe_wait_mode(DWORD state) {
    if ((state & PIPE_NOWAIT) != 0) {
        return L"Non-blocking";
    }
    return L"Blocking";
}

std::wstring sid_to_string(PSID sid) {
    LPWSTR sid_string = nullptr;
    if (::ConvertSidToStringSidW(sid, &sid_string)) {
        std::wstring result {sid_string};
        ::LocalFree(sid_string);
        return result;
    }
    return L"<unavailable>";
}

std::wstring lookup_account(PSID sid) {
    if (!sid || !::IsValidSid(sid)) {
        return L"<invalid sid>";
    }

    wchar_t name[256] = {0};
    wchar_t domain[256] = {0};
    DWORD name_size = static_cast<DWORD>(std::size(name));
    DWORD domain_size = static_cast<DWORD>(std::size(domain));
    SID_NAME_USE use {};

    if (::LookupAccountSidW(nullptr, sid, name, &name_size, domain, &domain_size, &use)) {
        std::wstring account;
        if (domain_size > 0) {
            account.assign(domain, domain_size);
            account.append(L"\\");
        }
        account.append(name, name_size);
        return account;
    }

    return L"<unresolved>";
}

std::wstring ace_type_to_string(const ACE_HEADER* header) {
    if (!header) {
        return L"Unknown";
    }
    switch (header->AceType) {
        case ACCESS_ALLOWED_ACE_TYPE:
            return L"ALLOW";
        case ACCESS_DENIED_ACE_TYPE:
            return L"DENY";
        default:
            return L"OTHER";
    }
}

std::wstring access_mask_to_string(DWORD mask) {
    std::wstring result;
    result.reserve(64);

    if ((mask & GENERIC_ALL) == GENERIC_ALL) {
        result.append(L"GENERIC_ALL ");
        mask &= ~GENERIC_ALL;
    }
    if ((mask & GENERIC_READ) == GENERIC_READ) {
        result.append(L"GENERIC_READ ");
        mask &= ~GENERIC_READ;
    }
    if ((mask & GENERIC_WRITE) == GENERIC_WRITE) {
        result.append(L"GENERIC_WRITE ");
        mask &= ~GENERIC_WRITE;
    }
    if ((mask & GENERIC_EXECUTE) == GENERIC_EXECUTE) {
        result.append(L"GENERIC_EXECUTE ");
        mask &= ~GENERIC_EXECUTE;
    }

    if (mask != 0) {
        std::wstringstream stream;
        stream << L"0x" << std::uppercase << std::hex << mask;
        result.append(stream.str());
    }

    if (!result.empty() && result.back() == L' ') {
        result.pop_back();
    }

    if (result.empty()) {
        result = L"<none>";
    }

    return result;
}

void print_acl(PACL acl) {
    if (!acl) {
        std::wcout << L"DACL: <none>\n";
        return;
    }

    ACL_SIZE_INFORMATION info {};
    if (!::GetAclInformation(acl, &info, sizeof(info), AclSizeInformation)) {
        std::wcout << L"DACL: <unavailable>\n";
        return;
    }

    std::wcout << L"DACL entries: " << info.AceCount << L"\n";

    for (DWORD index = 0; index < info.AceCount; ++index) {
        void* ace_ptr = nullptr;
        if (!::GetAce(acl, index, &ace_ptr)) {
            continue;
        }

        auto* header = static_cast<ACE_HEADER*>(ace_ptr);
        std::wstring type = ace_type_to_string(header);
        DWORD mask = 0;
        PSID sid = nullptr;

        if (header->AceType == ACCESS_ALLOWED_ACE_TYPE) {
            auto* ace = reinterpret_cast<ACCESS_ALLOWED_ACE*>(ace_ptr);
            mask = ace->Mask;
            sid = reinterpret_cast<PSID>(&ace->SidStart);
        } else if (header->AceType == ACCESS_DENIED_ACE_TYPE) {
            auto* ace = reinterpret_cast<ACCESS_DENIED_ACE*>(ace_ptr);
            mask = ace->Mask;
            sid = reinterpret_cast<PSID>(&ace->SidStart);
        }

        std::wcout << L"  [" << index << L"] " << type << L" " << lookup_account(sid) << L" (" << sid_to_string(sid)
                   << L") rights=" << access_mask_to_string(mask) << L"\n";
    }
}

} // namespace

int show_pipe_info(const std::wstring& pipe_name) {
    try {
        PipeClient pipe = PipeClient::connect(pipe_name, GENERIC_READ | GENERIC_WRITE | READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_ATTRIBUTE_NORMAL);

        DWORD flags = 0;
        DWORD out_buffer_size = 0;
        DWORD in_buffer_size = 0;
        DWORD max_instances = 0;
        if (!::GetNamedPipeInfo(pipe.native_handle(), &flags, &out_buffer_size, &in_buffer_size, &max_instances)) {
            logging::log_message(L"GetNamedPipeInfo", ::GetLastError());
        }

        DWORD state = 0;
        DWORD cur_instances = 0;
        DWORD max_collection = 0;
        DWORD collection_timeout = 0;
        wchar_t server_user[256] = {0};

        if (!::GetNamedPipeHandleStateW(
                pipe.native_handle(),
                &state,
                &cur_instances,
                &max_collection,
                &collection_timeout,
                server_user,
                static_cast<DWORD>(std::size(server_user)))) {
            const DWORD state_error = ::GetLastError();
            if (state_error == ERROR_INVALID_PARAMETER) {
                logging::log_message(L"GetNamedPipeHandleState (server impersonation unavailable)", state_error);
                if (!::GetNamedPipeHandleStateW(
                        pipe.native_handle(),
                        &state,
                        &cur_instances,
                        &max_collection,
                        &collection_timeout,
                        nullptr,
                        0)) {
                    logging::log_message(L"GetNamedPipeHandleState", ::GetLastError());
                }
            } else {
                logging::log_message(L"GetNamedPipeHandleState", state_error);
            }
        }

        std::wcout << L"Pipe name: " << pipe.qualified_name() << L"\n";
        std::wcout << L"Type: " << describe_pipe_type(flags) << L"\n";
        std::wcout << L"Read mode: " << describe_read_mode(state) << L"\n";
        std::wcout << L"Wait mode: " << describe_wait_mode(state) << L"\n";
        std::wcout << L"Current instances: " << cur_instances << L"\n";
        std::wcout << L"Max instances: " << max_instances << L"\n";
        std::wcout << L"Inbound quota (bytes): " << in_buffer_size << L"\n";
        std::wcout << L"Outbound quota (bytes): " << out_buffer_size << L"\n";
        std::wcout << L"Collect data timeout (ms): " << collection_timeout << L"\n";
        if (server_user[0] != L'\0') {
            std::wcout << L"Server user: " << server_user << L"\n";
        }

        PSID owner_sid = nullptr;
        PACL dacl = nullptr;
        PSECURITY_DESCRIPTOR security_descriptor = nullptr;
        DWORD security_status = ::GetSecurityInfo(
            pipe.native_handle(),
            SE_KERNEL_OBJECT,
            OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
            &owner_sid,
            nullptr,
            &dacl,
            nullptr,
            &security_descriptor);

        if (security_status == ERROR_SUCCESS) {
            std::wcout << L"Owner: " << lookup_account(owner_sid) << L" (" << sid_to_string(owner_sid) << L")\n";
            print_acl(dacl);
        } else {
            logging::log_message(L"GetSecurityInfo", security_status);
        }

        if (security_descriptor) {
            ::LocalFree(security_descriptor);
        }

        return EXIT_SUCCESS;
    } catch (const std::system_error& ex) {
        logging::log_system_error(L"Pipe info failed", ex);
        return EXIT_FAILURE;
    } catch (const std::exception& ex) {
        std::cerr << "Pipe info failed: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}

} // namespace pipetool
