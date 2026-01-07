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

# building
- Requires: ninja, MSVC, cmake
- From a VS developer command prompt, cmake --workflow debug-workflow




