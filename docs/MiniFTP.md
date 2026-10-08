# MiniFTP

`MiniFTP` is a small classic Intuition FTP client for AmigaOS 1.3. It is a
GUI application and uses `bsdsocket.library` only; it does not call internal
`amitcp13` stack APIs.

Version identity:

```text
MiniFTP v1.5 by Marcel Jaehne (c)2026
```

The window title is shortened to `MiniFTP v1.5`; the full author/version text is shown in the `Info` dialog.

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

- a Workbench-sized window starting below the screen title bar when space permits
- framed connection fields for host, port, user, and password
- password input is masked on screen
- `Connect` and `Disconnect` buttons
- a local path field and `Open` button for mounted-drive selection
- a left pane with local files and directories
- a right pane with FTP server files
- independent vertical scrollbars for both file panes
- a horizontal scrollbar below the FTP pane: click the arrows or drag along
  the track to reveal long file names and the remote path; scrolling changes
  only the display, not the names used for transfers
- local and remote directories are marked with `[DIR]`
- a `..` parent entry is always shown at the top of both file panes
- center transfer buttons:
  - buttons are backed by Intuition boolean gadgets
  - click files or directories to mark/unmark them for multi-entry operations
  - `->` uploads the selected local file/directory or all marked local entries recursively
  - `<-` downloads the selected remote file/directory or all marked remote entries recursively
  - `DIR +` opens a small input window and creates a remote directory with `MKD` if it does not already exist
  - `Delete` deletes the selected entries from the active local or FTP pane recursively after one confirmation
  - `Project -> Info` opens the version/about dialog
  - `Project -> Send to back` moves the window behind other Workbench windows
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

## Address book

Open `Address Book -> Open` to manage saved FTP connections. Each entry contains a display name, host, port, user, password, and initial remote path. `Use` copies an entry into the main connection fields without connecting automatically. `Save current` creates or updates an entry, and `Delete` removes it. The data file is `MiniFTP.addressbook` in the startup directory. Passwords are stored as plain text.

## Workflow

1. Enter host, optional port, user, and password. Empty Port uses FTP port 21. Decimal ports from 1 through 65535 are accepted.
2. Click `Open` and click a mounted drive to load its root into the local pane.
   The picker shows device and volume names, with `Up`/`Down` for longer lists.
   Close it or choose `Cancel` to keep the current path. Alternatively, enter a
   local path such as `RAM:` or `Work:Download` and press Enter.
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
12. Use `Project -> Info` to show the MiniFTP version/about dialog.

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
- immediate socket cleanup when disconnecting or closing, without waiting for `QUIT`

PASV mode is the only supported transfer mode. Directory navigation reuses the
existing control connection: `CWD` and `CDUP` do not reconnect or log in again.
Each remote list refresh opens one temporary PASV data socket and closes it
before reading the final `226`/`250` transfer reply. Uploads issue `TYPE I`
immediately before `STOR` and retain the existing post-send drain handling.
Uploads are checked by the FTP completion reply and server `SIZE` when
available. The file is not downloaded again for verification. A size check
cannot detect content corruption that leaves the file size unchanged. If SIZE
is unavailable, the final status explicitly reports the missing size check.

## Transfer performance settings

The `Transfer` menu offers these session-only settings:

- `Upload block: 128/512/1024/2048 bytes`: select the maximum payload per send
  call. The provisional default is 512 bytes; 128 remains available for
  compatibility comparisons. Calls are capped at the compiled IPC payload
  limit. Hardware measurements are required before choosing a tuned default.
- `Last transfer timings`: show cumulative milliseconds for data transfer,
  size checking, and directory listing since the last upload/download batch.
  Data timing includes file I/O, data-connection setup and final replies;
  size checking measures SIZE requests; lists include local scans
  and remote LISTs. Other directory commands and user pauses are not included,
  so these numbers are not the full batch wall-clock duration.

File reads/writes use an 8 KB buffer, falling back to 4 KB, 2 KB, then a static
2 KB buffer if allocations fail. Buffers are released on every success, error, and cancellation path. Downloads flush their remaining
buffer, check every write, close the file, and validate the FTP completion reply
and available size information before reporting success. The return value of
`Close()` is not interpreted because it is undefined before dos.library V36. On cancellation, buffered
but unwritten data is discarded; a partial destination file may remain.
Network receive requests remain at 2 KB, matching the inspected TheWire13 IPC
payload limit.

Uploads try nonblocking send first and wait only after backpressure or retryable
errors. Successful partial sends advance by the number of bytes actually
accepted. Progress is redrawn at most every 250 ms, separately from cancellation
checks. Upload-end handling is intentionally retained for TheWire13 testing.

Batch transfers use snapshots rather than reloading directories after each
file. Recursive uploads scan each entered local directory; recursive downloads
list each entered remote directory. The visible lists are refreshed at the end,
including recovery after errors and cancellation.

## Performance benchmark procedure

Create deterministic test data on the host (not on the Amiga):

```sh
python3 tools/transfer_fixture.py create /tmp/miniftp-bench --large-kib 1024
```

The `data` directory contains a large file, 200 small files, an empty file, and
nested files with sizes that do not align to transfer buffers. Use a smaller
`--large-kib` value when testing an Amiga with limited RAM.

A comparison binary from commit `276c7c8` has been built as
`build/MiniFTP-baseline-276c7c8`; the optimized build is `build/MiniFTP`.
The baseline also predates the uncommitted scrolling/name fixes, so transfer
comparisons should use short file names supported by both builds.

Compare the previous build with the new build on the same Amiga, network stack,
FTP server and storage. First measure the large file alone, then `small`, then
`nested`; use `RAM:` and the hard disk separately. For uploads, compare all four
block sizes. The baseline performs a full read-back while the current build
checks size only; record that difference when comparing total elapsed times. Repeat
runs and record full elapsed time plus `Transfer -> Last transfer timings`.

| Build | Direction | Dataset | Storage | Block | Verification | Wall time | Data ms | Size/check ms | Lists ms | Hash check |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Previous / new | Upload / download | Large / small / nested | RAM / disk | 128–2048 | Full / size | Pending | Pending | Pending | Pending | Pending |

After a complete round trip of the `data` tree, verify its contents on the host:

```sh
python3 tools/transfer_fixture.py verify /tmp/miniftp-bench /path/to/returned/data
```

Also test Disconnect during transfer, server stalls, a full
local disk, low RAM and repeated reconnects. Host tests cover buffer fallback,
partial reads/writes/sends, cleanup, size mismatches, empty files,
unsupported SIZE, nested traversal and reduced directory refreshes. They do not
measure real Amiga/network throughput. No hardware speedup has yet been measured.

## Limitations

- Operations run sequentially and process Disconnect, window close, redraw,
  resize, and Send to back events while working. Other actions are ignored
  until the operation finishes; connection/path fields are temporarily disabled.
- Socket waits check events in 100 ms slices. DNS cancellation uses the socket
  library's interrupt mask and depends on the network stack honoring it.
  Blocking filesystem calls cannot be interrupted while inside AmigaDOS.
- Local and remote lists grow with available memory. An incomplete list is
  reported and cannot be used to start a recursive selection operation.
- Remote `LIST` parsing is intentionally basic and optimized for common UNIX
  style listings. Lines beginning with `d` are treated as directories; uncertain
  entries are treated as files.
- Recursive upload, download, and delete are supported for selected files and directories.
  Recursive loops and large list sorting/copying check for cancellation.
- Delete uses the last active file pane: click a local entry to delete locally, or
  click a remote entry to delete via FTP. The confirmation appears once at the start.
- No TLS/FTPS.
- No rename or active `PORT` mode.
- No Workbench AppWindow or drag-and-drop support on AmigaOS 1.3.
- A classic menu bar provides `Project -> Info`; the main actions use real Intuition buttons and double-clicks.
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

The GUI opens on the Workbench screen, leaving the screen title bar accessible
when the minimum window height permits. It uses old-style `NewWindow` / `OpenWindow`, plain Intuition string gadgets, and
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
15. Use `Project -> Info` and verify the version/about dialog opens and closes.
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

## Changelog

### 1.5

Added:

- An Open button replacing Load, with a mounted-drive picker for the local
  pane. Select a drive to open its root directory; device and volume names are
  shown together. The picker supports paging, Cancel, Escape and window close.
- Independent horizontal scrollbars with arrows and draggable sliders for both
  file panes. FTP scrolling also reveals long remote paths.
- A Transfer menu with selectable upload blocks of 128, 512, 1024 or 2048 bytes
  and separate timings for data transfer, size checks and directory listing.
- Reproducible benchmark files with SHA-256 validation, plus regression tests
  for buffered transfers, recursive batches, drive enumeration and long names.
- A `make test` target running all five host-side test suites.

Changed:

- Upload and download file I/O uses an 8 KB buffer, with 4 KB and 2 KB allocation
  fallbacks and a static 2 KB fallback for low-memory conditions.
- Uploads try nonblocking sends before waiting for socket capacity. Partial
  sends are handled correctly; the provisional default upload block is 512 bytes.
- Removed the extra upload download-and-compare pass. FTP completion replies
  and server SIZE checks remain; unavailable SIZE checks are reported explicitly.
- Batch transfers refresh visible directories at completion instead of after
  every file. Recursive operations retain the scans needed for traversal.
- Progress redraws are limited to one every 250 ms, independently of abort
  handling. Scrolling redraws only the affected pane and related scrollbar/path.
- FTP file names up to 255 bytes are preserved. Directory path buffers now hold
  up to 511 bytes, with larger FTP command, listing and address-book buffers.
- Enter in the local path field opens a manually entered directory.
- Loading an FTP directory displays `Listing Directory...` during data
  connection setup as well as listing reception.

Fixed:

- Removed the invalid check of the pre-V36 `Close()` return value that caused
  false `Local file close failed` errors and disconnections on AmigaOS 1.3.
  Buffered writes, FTP completion replies and available size checks remain.
- Long FTP names are no longer silently truncated to 63 bytes. Unsupported
  names, oversized listing lines and paths are rejected or reported explicitly.
- Downloads use the exact remote file name instead of interpreting it as a
  local path and retaining only its suffix.
- Scrollbar movement no longer refreshes the entire MiniFTP window.
- Transfer cleanup preserves error messages, releases buffers and snapshots,
  and restores the local directory after failures or cancellation.

Validation and limitations:

- The Amiga cross-build completes without compiler warnings. All five host
  test suites pass, including memory/error handling and OS 1.3 Close behavior.
- The new drive picker and optimized transfers still require runtime acceptance
  on AmigaOS 1.3. No real-hardware throughput improvement has yet been measured.
- Name and path limits are measured in bytes; local filesystem limits still
  apply. Size checks cannot detect content corruption at the same file size.
- Existing DNS cancellation limitations depend on the installed network stack.

### 1.4

Added:

- A Disconnect button to cancel active operations and close the FTP connection.
- `Project -> Send to back`, available while network operations are running.
- Host-side regression tests for dynamic lists, memory allocation failures,
  cancellation, socket waits, and window layout.

Changed:

- Local and remote directory lists now grow with available memory instead of
  stopping at 128 entries. Recursive transfers and deletion use dynamically
  allocated snapshots as well.
- Incomplete directory lists are reported explicitly and cannot be used to
  start recursive selection operations.
- Socket waits process GUI events in 100 ms intervals. Transfers, upload
  verification, recursive operations, and large-list sorting/copying check
  for cancellation. Closing the main window also requests cancellation.
- Disconnect and exit close sockets without waiting for a FTP `QUIT` reply.
  Cancelled transfers may leave partial files; completed changes are not
  rolled back.
- Startup sizing uses the Workbench screen dimensions and leaves its title
  bar accessible when space permits.
- The minimum window size is now 560x186, with more compact transfer buttons.

Fixed:

- Redrawing no longer clears the Intuition window borders and system gadgets.
- The LOAD button, remote file pane, and status box use the actual right
  border width and remain inside the surrounding frame.
- File names and status text are clipped to their respective display areas.
- The LOAD button has been moved up by one pixel for alignment.
- Recursive local deletion stops on directory read errors instead of treating
  an incomplete directory listing as complete.

Known limitations:

- DNS cancellation depends on the socket library honoring interrupt signals.
  The locally inspected TheWire13 v1.8.3 resolver does not yet check these
  signals, so cancellation during DNS lookup waits for the lookup to return.
  Connecting to an IPv4 address bypasses DNS.
- A blocking AmigaDOS filesystem call cannot be interrupted until it returns.
- The cross-build and host regression tests pass. Full runtime acceptance
  on AmigaOS 1.3 remains pending.

## Planned changes

- [ ] Replace the fixed 128-entry limits for local and remote directory lists
  with dynamically growing lists, limited by available RAM. Apply this to
  recursive upload, download, and delete operations and their entry snapshots
  as well. Report insufficient memory explicitly instead of silently omitting
  entries, and release allocated memory when it is no longer needed.
- [ ] Make switching to other programs and the Workbench easily accessible
  after startup. Review the full-screen initial window size, preserve Intuition
  window borders and system gadgets when redrawing, and provide an explicit
  menu action to send MiniFTP to the back. Verify depth and resize gadgets and
  program switching on AmigaOS 1.3; the reported inability to switch has not
  yet been reproduced.
- [ ] Add a Disconnect button that cancels an ongoing operation and closes
  the FTP connection, including when a transfer stalls or the user changes
  their mind. Process cancellation during transfers, recursive operations,
  connection setup, and waits for server replies so the button remains
  responsive. Close control and data sockets, release operation resources,
  report cancellation clearly, and return the GUI to a usable disconnected
  state from which the user can reconnect. Handle partially transferred files
  explicitly rather than reporting them as complete.

### Implementation progress

All three changes are implemented for testing; the checklist remains open
until runtime acceptance on AmigaOS 1.3.

- Window geometry now uses the Workbench screen and leaves its title bar
  accessible when space permits. Redrawing respects window borders, and
  `Project -> Send to back` is available during network operations as well.
  LOAD, the remote pane, and the status box share a right edge calculated
  from the actual Intuition border width. File names and status text are
  clipped to their own areas.
  The minimum window size is 560x186; the transfer buttons are more compact.
- Directory lists and recursive snapshots grow dynamically. Allocation failures
  preserve existing allocations, report incomplete lists, and stop operations
  that require a complete directory. Allocation sizes are tracked separately
  from entry counts so snapshots can always be freed correctly.
- Disconnect cancels socket waits, streaming transfers, size checks, and
  recursive operations. Cleanup closes sockets without waiting for a server
  response. Closing the main window during an operation requests cancellation
  before exiting. Partial local/remote files may remain; no rollback is made.
- DNS lookup requests interruption on GUI events through `SetSocketSignals`.
  Runtime verification with the actual network stack is still required; a stack
  that ignores that mask may block until its resolver returns.

Validation performed:

- Cross-build with `-Wall -Wextra`.
- `python3 tests/test_core.py`: actual C helper functions are compiled on the
  host with substituted Amiga APIs and address/undefined-behavior sanitizers.
  Covers 5,001 entries, sorting, selected snapshots, allocation failures,
  smaller-allocation fallback, allocation/free size matching, cancellation,
  socket readiness, timeouts, and interrupted/error waits.

- `python3 tests/test_layout.py`: production layout and text clipping checked
  at 36 combinations of window size and right-border width, including the
  wider border used by the sizing gadget.

Runtime acceptance still required:

1. On AmigaOS 1.3, resize, switch windows/screens, send MiniFTP to the back,
   return to it, and verify typing in every connection/path field.
2. List and recursively transfer/delete directories containing more than 128
   entries, including nested directories; compare results with the source.
3. Disconnect during connect, DNS, LIST, upload, download, size checks, and
   recursive operations; repeat with a stalled server and reconnect afterward.
4. Close the main window during an operation and check that it exits cleanly.
5. Repeat large-list operations with limited RAM and verify memory recovery.
