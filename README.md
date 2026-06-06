# MiniFTP

MiniFTP is a classic AmigaOS 1.3 Intuition FTP client.

Version:

```text
MiniFTP v1.1 by Marcel Jaehne (c)2026
```

It was split out of TheWire13 and remains designed for Kickstart/Workbench 1.3,
68000, and the bebbo `m68k-amigaos-gcc` toolchain. Networking uses a classic
`bsdsocket.library` API directly; MiniFTP does not call TheWire13 internal stack
APIs.

## Features

- Plain Intuition GUI, no GadTools/MUI/ReAction/ASL.
- Starts fullscreen on the current Workbench screen.
- Dynamically resizable two-pane local/remote file browser.
- Shell and Workbench startup support.
- Workbench ToolTypes for host, user, password, local path, remote path,
  autoconnect, and port.
- Configurable FTP control port.
- Persistent FTP control connection.
- PASV `LIST`, `RETR`, `STOR`, and `DELETE` support.
- Local and remote directory navigation with `..` entries.

## Requirements

- AmigaOS 1.3 / Kickstart 1.3.
- 68000-compatible build.
- `bsdsocket.library` at runtime.
- Toolchain: `/opt/amiga/bin/m68k-amigaos-gcc`.

## Build

```sh
make clean && make
```

Output:

```text
build/MiniFTP
```

## Documentation

See [docs/MiniFTP.md](docs/MiniFTP.md).
