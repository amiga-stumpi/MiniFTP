# MiniFTP

`MiniFTP` is a small classic Intuition FTP client for AmigaOS 1.3. It is a
GUI application and uses `bsdsocket.library` only; it does not call internal
`amitcp13` stack APIs.

Version identity:

```text
MiniFTP v1.2 by Marcel Jaehne (c)2026
```

The window title is shortened to `MiniFTP v1.2`; the full author/version text is shown in the `Info` dialog.

## Shell Usage

Start `TheWire13`, install `bsdsocket.library`, then run:

```text
MiniFTP
```

The Shell launch path keeps the existing defaults:

- host field empty
- user `anonymous`
- password `test@example.com`
- local path `RAM:`
- remote path `/`
- port field empty, which uses FTP port 21

The window contains:

- a 639x200 classic Workbench-safe window with an application frame
- framed connection fields for host, port, user, and password
- password input is masked on screen
- a `Connect` button
- a local path field and `Load` button
- a left pane with local files and directories
- a right pane with FTP server files
- independent vertical scrollbars for both file panes
- local and remote directories are marked with `[DIR]`
- a `..` parent entry is always shown at the top of both file panes
- center transfer buttons:
  - buttons are backed by Intuition boolean gadgets
  - click files or directories to mark/unmark them for multi-entry operations
  - `->` uploads the selected local file/directory or all marked local entries recursively
  - `<-` downloads the selected remote file/directory or all marked remote entries recursively
  - `DIR +` opens a small input window and creates a remote directory with `MKD` if it does not already exist
  - `Delete` deletes the selected entries from the active local or FTP pane recursively after one confirmation
  - `Projekt -> Info` opens the version/about dialog
- a status/error line for connection failures, timeouts, and transfer progress

## Workbench Usage

`MiniFTP` can also be started by double-clicking its Workbench icon. The
program detects Workbench startup, reads ToolTypes from the program icon through
`icon.library`, opens the same Intuition window, and does not require normal
Shell output.

Supported ToolTypes:

```text
HOST=ftp.example.com
USER=anonymous
PASSWORD=test@example.com
LOCALPATH=RAM:
REMOTEPATH=/
AUTOCONNECT=NO
PORT=21
```

Notes:

- `PASSWORD` is optional. If omitted during Workbench startup, the password field
  is left empty.
- `AUTOCONNECT=YES` connects automatically when both `HOST` and `USER` are set.
- `PORT` pre-fills the Port field. Empty or whitespace-only uses 21. Invalid values such as `0`, `65536`, `abc`, `21abc`, or `-1` are rejected with `Invalid port` and no socket is opened.
- Password values are copied into the masked password field and are not printed
  in debug logs.

To create an icon, copy an existing AmigaOS 1.3 tool icon to
`MiniFTP.info`, set the default tool to `MiniFTP`, and add the
ToolTypes above in Workbench icon information. A binary `.info` file is not
required in the source tree.

No AppWindow, drag-and-drop, ASL requester, GadTools, ReAction, or MUI support is
used. Those APIs are intentionally avoided for AmigaOS 1.3 compatibility.

## Workflow

1. Enter host, optional port, user, and password. Empty Port uses FTP port 21. Decimal ports from 1 through 65535 are accepted.
2. Enter a local path, such as `RAM:` or `Work:Download`, then click `Load`.
3. Click `Connect`.
4. The client logs in, sends `TYPE I`, and loads the remote directory with
   PASV `LIST`.
5. Double-click a local directory in the left pane to enter it.
6. Double-click local `..` to move one AmigaDOS directory level up.
7. Select a local file or directory and click `->` to upload it. Directories are
   created remotely and processed recursively; `..` is protected.
8. Select a remote file or directory and click `<-` to download it into the
   current local path. Remote directories are created locally and processed recursively.
9. Select local or remote entries and click `Delete`; confirm the single safety prompt.
   Local trees are removed with AmigaDOS `DeleteFile()`, remote trees with `DELE`/`RMD`.
10. Double-click a remote directory to enter it with `CWD`.
11. Double-click remote `..` to go up with `CDUP`.
12. Use `Projekt -> Info` to show the MiniFTP version/about dialog.

## FTP Support

The GUI supports:

- TCP control connection through `bsdsocket.library`
- `USER` / `PASS`
- binary mode with `TYPE I`
- one persistent FTP control connection per login session
- one short-lived PASV data connection per `LIST`, `RETR`, or `STOR`
- remote `LIST`
- local directory navigation in the left pane using plain AmigaDOS paths
- remote directory navigation with `CWD` / `CDUP`
- upload with `STOR`, including recursive directory upload via `MKD`/`CWD`
- explicit remote directory creation with `MKD` through the `DIR +` button
- download with `RETR`, including recursive directory download
- remote delete with `DELE` for files and `RMD` for emptied directories
- local recursive delete with `DeleteFile()`
- `QUIT` when closing an active session

PASV mode is the only supported transfer mode. Directory navigation reuses the
existing control connection: `CWD` and `CDUP` do not reconnect or log in again.
Each remote list refresh opens one temporary PASV data socket and closes it
before reading the final `226`/`250` transfer reply. Uploads send conservative 128-byte file chunks, issue `TYPE I` immediately before
`STOR`, wait briefly for the data socket to become writable after the final file
block before closing it, compare server `SIZE` with the local byte count when
the server supports `SIZE`, and then read the uploaded file back with `RETR` for
a byte-for-byte verification pass. This avoids closing a nonblocking stack while
the last accepted bytes are still being flushed and catches truncated or
corrupted uploads.

## Limitations

- Version 0.8 keeps transfers synchronous; the UI may pause during connect,
  listing, upload, or download. Transfer progress and simple status changes
  redraw only the status line; full UI redraws are reserved for list/path
  changes and refresh events.
- The custom list panes support simple vertical scrolling, double-click
  directory navigation, and independent local/remote selections. The current
  entry limit is 128 per pane.
- Remote `LIST` parsing is intentionally basic and optimized for common UNIX
  style listings. Lines beginning with `d` are treated as directories; uncertain
  entries are treated as files.
- Recursive upload, download, and delete are supported for selected files and directories.
  The implementation keeps operations synchronous, so very large directory trees can pause the UI.
- Delete uses the last active file pane: click a local entry to delete locally, or
  click a remote entry to delete via FTP. The confirmation appears once at the start.
- No TLS/FTPS.
- No rename, mkdir, or active `PORT` mode.
- No Workbench AppWindow or drag-and-drop support on AmigaOS 1.3.
- A classic menu bar provides `Projekt -> Info`; the main actions use real Intuition buttons and double-clicks.
- The `Info` dialog uses a plain OS1.3 Intuition window, not ASL/GadTools.

## Connection Status

The status line is the primary user-facing error display. It is updated during
connection setup:

```text
Connecting...
Resolving host...
Connecting control socket...
Waiting for greeting...
Sending USER...
Sending PASS...
Setting binary mode...
Connected
```

Failures such as `Resolve failed`, `Connect failed errno=N`, `Timeout waiting
for server`, `FTP greeting failed`, `Login failed`, `LIST failed`, `Upload
failed`, `Download failed`, `FTP delete failed`, `Permission denied`, and
`Remote file not found` are shown in the GUI status area rather than only in
Shell debug output.

Connection and transfer failures are cleaned up before control returns to the
event loop:

- a failed `Connect` closes any partial control socket and clears the remote
  list, so a second `Connect` starts from a fresh session
- successful login keeps one control socket open for the whole GUI session
- before every PASV transfer/list, any previous tracked data socket is closed
- every PASV data socket is closed on success, timeout, short read, server
  error, and local file error paths
- after a successful `LIST`, `RETR`, or `STOR`, the GUI closes the data socket
  and waits for the final FTP `226`/`250` control reply
- an upload/download/list failure after `STOR`, `RETR`, or `LIST` may leave the
  FTP control channel ambiguous, so the GUI closes the session and shows
  `reconnect required`
- failed deletes or directory changes that lose the control reply also close the
  session and require reconnecting

After any `reconnect required` status, click `Connect` again before attempting
GET, PUT, Delete, or directory navigation.

When built with `MINI_FTP_DEBUG=1`, lifecycle diagnostics are printed to the
Shell while debugging connection or upload failures:

- Workbench/startup phase markers
- window geometry markers
- control socket open/close fd
- data socket open/close fd
- raw and parsed PASV endpoint
- `LIST`, `CWD`, and `CDUP` start/result markers
- initial `Connect()` result/errno and `WaitSelect()` connect completion
- first upload `Send()` return value/errno

`Errno=55` is `ENOBUFS`, which points at socket/PCB allocation pressure.
`Errno=5` is `EIO`; during PUT it may be a temporary send-buffer condition and
is retried rather than treated as immediately fatal. MiniFTP allows extra
temporary PUT retries on the PASV data socket so short TX backpressure does not
abort uploads immediately. TheWire13 `send()` may return a short positive count
when only part of the upload buffer fits; MiniFTP keeps the remaining bytes and
continues after `WaitSelect()` reports write readiness. Downloads issue `SIZE` before `RETR` when the server supports it; if the data socket closes before the announced byte count is received, MiniFTP reports an incomplete download and reconnects instead of showing a false completion.

## Troubleshooting Startup

For normal Workbench use, startup failures are shown in the GUI status line when
the window can be opened. If the program exits before the window appears, build
with `MINI_FTP_DEBUG=1` and run from Shell to print startup phase diagnostics.

Possible startup failures include:

- `intuition.library open failed`
- `graphics.library open failed`
- `bsdsocket.library open failed`
- `window open failed`
- `visual/screen issue or not enough memory`

The GUI opens on the current Workbench screen at the visible screen size and
uses old-style `NewWindow` / `OpenWindow`, plain Intuition string gadgets, and
custom text panes. The window has an OS1.3-safe sizing gadget; on resize MiniFTP
recalculates the two file panes, scrollbars, transfer buttons, and bottom status
line. The string gadgets are attached only after the window has opened. It does
not use GadTools, tag-based window APIs, VisualInfo, public-screen APIs, or
ListView gadgets.

## Test Plan

Recommended hardware tests:

```text
MiniFTP
```

Then:

1. Connect to a local FTP server.
2. Load `RAM:` or another local directory.
3. Confirm remote `LIST` fills the right pane.
4. Upload one or more selected local files.
5. Download one or more selected remote files.
6. Scroll both file panes when more than eight entries are present.
7. Confirm local and remote directories display as `[DIR] name`.
8. Double-click a local directory and confirm the local path/list update.
9. Double-click local `..` and confirm the parent directory loads; at volume root
   the status line should show `Already at volume root`.
10. Select a local directory and confirm recursive upload creates the remote directory tree.
11. Double-click a remote directory and confirm the remote path/list update.
12. Double-click remote `..` and confirm the parent directory loads.
13. Select multiple local or remote entries, delete them, confirm one prompt appears, and confirm the list refreshes.
14. Delete a selected local file, confirm the prompt, and confirm the local list refreshes.
15. Use `Projekt -> Info` and verify the version/about dialog opens and closes.
16. Try an unreachable/wrong host, confirm `Connect failed: timeout` or a clear
    error is shown, then connect to a valid server without restarting the GUI.
17. Interrupt or provoke a failed upload, confirm the GUI returns to the event
    loop and either GET still works or `reconnect required` is shown.
18. Start from Workbench with ToolTypes, confirm fields are prefilled and
    `AUTOCONNECT=YES` connects when `HOST` and `USER` are present.
19. Confirm the CLI client still works:

```text
mini_ftp 192.168.7.1 anonymous test@example.com list
mini_ftp 192.168.7.1 anonymous test@example.com get readme.txt ram:readme.txt
mini_ftp 192.168.7.1 anonymous test@example.com put ram:test.txt
```
