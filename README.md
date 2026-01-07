# pipetool

This is a quick-and-dirty tool to execute a couple of simple workflows on a named pipe. I used codex 5.1 to write the entire thing.

# usage

```
Usage: pipetool <pipename> <subcommand> [options]

Subcommands:
  --stream-file <path>   Stream the entire file into the pipe.
  --fuzz [bytes]         Send random payloads (default 100 bytes).
  --info                 Display security-related pipe metadata.
```

# examples

```
C:\>pipetool com.contoso.mypipe --stream-file c:\temp\moby_dick.txt
[0] File sent - OK
[109] Pipe connection closed - The pipe has been ended.

C:\>pipetool com.contoso.mypipe --info
[87] GetNamedPipeHandleState (server impersonation unavailable) - The parameter is incorrect.
[87] GetNamedPipeHandleState - The parameter is incorrect.
Pipe name: \\.\pipe\com.contoso.mypipe
Type: Message
Read mode: Unknown
Wait mode: Blocking
Current instances: 2
Max instances: 255
Inbound quota (bytes): 4096
Outbound quota (bytes): 4096
Collect data timeout (ms): 0
Owner: BUILTIN\Administrators (S-1-5-32-544)
DACL entries: 4
  [0] DENY BUILTIN\Guests (S-1-5-32-546) rights=0x1F01FF
  [1] DENY NT AUTHORITY\ANONYMOUS LOGON (S-1-5-7) rights=0x1F01FF
  [2] DENY NT AUTHORITY\NETWORK (S-1-5-2) rights=0x1F01FF
  [3] ALLOW NT AUTHORITY\Authenticated Users (S-1-5-11) rights=0x12019F
```

# building
- Requires: ninja, MSVC, cmake
- From a VS developer command prompt, cmake --workflow debug-workflow




