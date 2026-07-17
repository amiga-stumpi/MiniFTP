# MiniFTP

MiniFTP is a classic AmigaOS 1.3 Intuition FTP client.

Version:

```text
MiniFTP v1.3 by Marcel Jaehne (c)2026
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
- PASV `LIST`, `RETR`, `STOR`, and remote `DELE` support.
- Real Intuition button gadgets for the main actions.
- Multiple files and directories can be marked for sequential recursive upload/download.
- Local and remote files/directories can be deleted recursively after one confirmation.
- Remote directories can be created from the `DIR +` button via FTP `MKD`.
- Local and remote directory navigation with `..` entries.
- Persistent FTP address book available from the menu bar.
- Address book entries store name, host, port, user, password, and remote path.
- Saved address book entries can be loaded into the connection fields, updated, or deleted.
- Info dialog with author/version text.

## Address book

Open the address book from `Address Book -> Open`. `Save current` stores the current host, port, user, password, and remote path under the entered name. `Use` copies the selected entry back into the main connection fields without connecting automatically. Entries can be updated by saving the same name again or removed with `Delete`.

Entries are stored in `MiniFTP.addressbook` in the startup directory. Passwords are stored as plain text, matching the existing configuration and ToolType behavior. Protect the file accordingly.

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
