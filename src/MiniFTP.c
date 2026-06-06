#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <dos/dos.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/icon.h>

#include "amitcp13/bsdsocket.h"
#include "amitcp13/stack_ipc.h"

#define MINI_FTP_VERSION "v1.1"
#define MINI_FTP_GUI_TITLE "MiniFTP " MINI_FTP_VERSION
#define MINI_FTP_FULL_ID "MiniFTP " MINI_FTP_VERSION " by Marcel Jaehne (c)2026"
#define FTP_PORT 21
#define TIMEOUT_SECONDS 20
#define LINE_BUF_SIZE 512
#ifndef AMITCP13_SOCKET_RECV_CHUNK
#define AMITCP13_SOCKET_RECV_CHUNK 2048
#endif
#define DATA_BUF_SIZE AMITCP13_SOCKET_RECV_CHUNK
#define CMD_BUF_SIZE 512
#define HOST_BUF_SIZE 96
#define PORT_BUF_SIZE 8
#define USER_BUF_SIZE 48
#define PASS_BUF_SIZE 48
#define PATH_BUF_SIZE 160
#define NAME_BUF_SIZE 64
#define MAX_LOCAL_ENTRIES 128
#define MAX_REMOTE_ENTRIES 128
#define PROGRESS_STEP_BYTES 32768L
#define UPLOAD_RETRY_LIMIT 64
#define CONTROL_SEND_RETRY_LIMIT 64
#define UPLOAD_DRAIN_POLLS 20
#define UPLOAD_CHUNK_SIZE 512
#define BSD_EAGAIN_COMPAT 35

#ifndef MINI_FTP_DEBUG
#define MINI_FTP_DEBUG 0
#endif

#define SOL_SOCKET AMITCP13_SOL_SOCKET
#define SO_ERROR   AMITCP13_SO_ERROR
#define FIONBIO    AMITCP13_FIONBIO

#define GID_HOST 1
#define GID_USER 2
#define GID_PASS 3
#define GID_PORT 4
#define GID_PATH 5

#define ROW_H 9
#define SCROLL_W 10

#define SCROLL_NONE 0
#define SCROLL_LOCAL 1
#define SCROLL_REMOTE 2

#define BUTTON_NONE 0
#define BUTTON_CONNECT 1
#define BUTTON_LOAD 2
#define BUTTON_UPLOAD 3
#define BUTTON_DOWNLOAD 4
#define BUTTON_DELETE 5
#define BUTTON_INFO 6

#define ACTIVE_PANE_NONE 0
#define ACTIVE_PANE_LOCAL 1
#define ACTIVE_PANE_REMOTE 2

#define CONTROL_H 84
#define STATUS_H 20
#define MIN_LIST_ROWS 4

struct FtpEntry {
    char name[NAME_BUF_SIZE];
    UBYTE is_dir;
};

static struct Window *g_win;
static struct Library *g_sock_base;
struct Library *IconBase;
static int g_ctrl_fd = -1;
static int g_data_fd = -1;
static int g_connected;
static char g_line[LINE_BUF_SIZE];
static char g_cmd[CMD_BUF_SIZE];
static UBYTE g_data_buf[DATA_BUF_SIZE];
static char g_host[HOST_BUF_SIZE] = "";
static char g_port[PORT_BUF_SIZE] = "";
static char g_user[USER_BUF_SIZE] = "anonymous";
static char g_pass[PASS_BUF_SIZE] = "test@example.com";
static char g_local_path[PATH_BUF_SIZE] = "RAM:";
static char g_remote_path[PATH_BUF_SIZE] = "/";
static char g_status[128] = "Not connected";
static char g_host_undo[HOST_BUF_SIZE];
static char g_port_undo[PORT_BUF_SIZE];
static char g_user_undo[USER_BUF_SIZE];
static char g_path_undo[PATH_BUF_SIZE];
static struct FtpEntry g_local_entries[MAX_LOCAL_ENTRIES];
static struct FtpEntry g_remote_entries[MAX_REMOTE_ENTRIES];
static int g_local_count;
static int g_remote_count;
static int g_local_sel = -1;
static int g_remote_sel = -1;
static int g_local_top;
static int g_remote_top;
static int g_scroll_drag;
static int g_pressed_button;
static int g_active_pane = ACTIVE_PANE_REMOTE;
static int g_pass_active;
static int g_transfer_busy;
static int g_last_local_click_index = -1;
static ULONG g_last_local_click_seconds;
static ULONG g_last_local_click_micros;
static int g_last_remote_click_index = -1;
static ULONG g_last_remote_click_seconds;
static ULONG g_last_remote_click_micros;
static struct Amitcp13BsdFdSet g_read_fds;
static struct Amitcp13BsdFdSet g_write_fds;
static struct Amitcp13BsdTimeVal g_timeout;
static struct Amitcp13BsdSockAddrIn g_addr;
static ULONG g_wait_signals;
static LONG g_one = 1;
static int g_so_error;
static int g_so_error_len;
static WORD BTN_CONNECT_X = 235;
static WORD BTN_CONNECT_Y = 48;
static WORD BTN_CONNECT_W = 76;
static WORD BTN_CONNECT_H = 14;
static WORD BTN_LOAD_X = 520;
static WORD BTN_LOAD_Y = 16;
static WORD BTN_LOAD_W = 42;
static WORD BTN_LOAD_H = 14;
static WORD BTN_UPLOAD_X = 300;
static WORD BTN_UPLOAD_Y = 94;
static WORD BTN_UPLOAD_W = 38;
static WORD BTN_UPLOAD_H = 18;
static WORD BTN_DOWNLOAD_X = 300;
static WORD BTN_DOWNLOAD_Y = 122;
static WORD BTN_DOWNLOAD_W = 38;
static WORD BTN_DOWNLOAD_H = 18;
static WORD BTN_DELETE_X = 294;
static WORD BTN_DELETE_Y = 146;
static WORD BTN_DELETE_W = 62;
static WORD BTN_DELETE_H = 14;
static WORD BTN_INFO_X = 294;
static WORD BTN_INFO_Y = 166;
static WORD BTN_INFO_W = 62;
static WORD BTN_INFO_H = 14;
static WORD LOCAL_X = 10;
static WORD LOCAL_Y = 94;
static WORD LOCAL_W = 270;
static WORD LOCAL_H = 76;
static WORD REMOTE_X = 360;
static WORD REMOTE_Y = 94;
static WORD REMOTE_W = 268;
static WORD REMOTE_H = 76;
static WORD PASS_FIELD_X = 55;
static WORD PASS_FIELD_Y = 48;
static WORD PASS_FIELD_W = 170;
static WORD PASS_FIELD_H = 12;
static WORD STATUS_X = 8;
static WORD STATUS_Y = 173;
static WORD STATUS_W = 622;
static WORD STATUS_TEXT_Y = 187;
static int g_visible_rows = 8;

static int clamp_top(int top, int count);
static int in_rect(WORD mx, WORD my, WORD x, WORD y, WORD w, WORD h);
static void clear_rect(WORD x, WORD y, WORD w, WORD h);
static void draw_password_field(void);
static void show_info_dialog(void);

static struct StringInfo g_host_si = { (STRPTR)g_host, (STRPTR)g_host_undo, 0, HOST_BUF_SIZE, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct StringInfo g_port_si = { (STRPTR)g_port, (STRPTR)g_port_undo, 0, PORT_BUF_SIZE, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct StringInfo g_user_si = { (STRPTR)g_user, (STRPTR)g_user_undo, 0, USER_BUF_SIZE, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct StringInfo g_path_si = { (STRPTR)g_local_path, (STRPTR)g_path_undo, 0, PATH_BUF_SIZE, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static struct Gadget g_path_gad = {
    0, 360, 16, 150, 12, GFLG_GADGHCOMP, GACT_RELVERIFY | GACT_STRINGLEFT,
    GTYP_STRGADGET, 0, 0, 0, 0, (APTR)&g_path_si, GID_PATH, 0
};
static struct Gadget g_port_gad = {
    &g_path_gad, 245, 16, 48, 12, GFLG_GADGHCOMP, GACT_RELVERIFY | GACT_STRINGLEFT,
    GTYP_STRGADGET, 0, 0, 0, 0, (APTR)&g_port_si, GID_PORT, 0
};
static struct Gadget g_user_gad = {
    &g_port_gad, 55, 32, 170, 12, GFLG_GADGHCOMP, GACT_RELVERIFY | GACT_STRINGLEFT,
    GTYP_STRGADGET, 0, 0, 0, 0, (APTR)&g_user_si, GID_USER, 0
};
static struct Gadget g_host_gad = {
    &g_user_gad, 55, 16, 135, 12, GFLG_GADGHCOMP, GACT_RELVERIFY | GACT_STRINGLEFT,
    GTYP_STRGADGET, 0, 0, 0, 0, (APTR)&g_host_si, GID_HOST, 0
};

static struct NewWindow g_new_window = {
    0, 0, 639, 200,
    0, 1,
    IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_NEWSIZE | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_GADGETUP | IDCMP_VANILLAKEY,
    WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_SIZEGADGET | WFLG_SIZEBRIGHT | WFLG_SIZEBBOTTOM | WFLG_ACTIVATE | WFLG_SMART_REFRESH,
    0,
    0,
    (STRPTR)MINI_FTP_GUI_TITLE,
    0,
    0,
    460, 150,
    1000, 600,
    WBENCHSCREEN
};

static void set_initial_window_size(void)
{
    struct Screen *screen = 0;
    WORD width;
    WORD height;

    if (IntuitionBase) {
        screen = IntuitionBase->ActiveScreen;
        if (!screen)
            screen = IntuitionBase->FirstScreen;
    }
    if (!screen)
        return;

    width = screen->Width;
    height = screen->Height;
    if (width < g_new_window.MinWidth)
        width = g_new_window.MinWidth;
    if (height < g_new_window.MinHeight)
        height = g_new_window.MinHeight;

    g_new_window.LeftEdge = 0;
    g_new_window.TopEdge = 0;
    g_new_window.Width = width;
    g_new_window.Height = height;
    g_new_window.MaxWidth = width;
    g_new_window.MaxHeight = height;
}

static void update_layout(void)
{
    WORD win_w = g_win ? g_win->Width : g_new_window.Width;
    WORD win_h = g_win ? g_win->Height : g_new_window.Height;
    WORD list_top = CONTROL_H;
    WORD mid_w = 70;
    WORD gap = 10;
    WORD usable_w;
    WORD pane_w;
    WORD status_h = STATUS_H;
    WORD path_x;
    WORD load_x;

    if (win_w < g_new_window.MinWidth)
        win_w = g_new_window.MinWidth;
    if (win_h < g_new_window.MinHeight)
        win_h = g_new_window.MinHeight;

    g_host_gad.LeftEdge = 55;
    g_host_gad.TopEdge = 16;
    g_host_gad.Width = (win_w >= 620) ? 135 : 120;
    g_host_gad.Height = 12;
    g_port_gad.LeftEdge = (WORD)(g_host_gad.LeftEdge + g_host_gad.Width + 55);
    g_port_gad.TopEdge = 16;
    g_port_gad.Width = 48;
    g_port_gad.Height = 12;
    g_user_gad.LeftEdge = 55;
    g_user_gad.TopEdge = 32;
    g_user_gad.Width = 170;
    g_user_gad.Height = 12;
    PASS_FIELD_X = 55;
    PASS_FIELD_Y = 48;
    PASS_FIELD_W = 170;
    PASS_FIELD_H = 12;

    BTN_CONNECT_X = 235;
    BTN_CONNECT_Y = 48;
    BTN_CONNECT_W = 76;
    BTN_CONNECT_H = 14;

    load_x = (WORD)(win_w - 58);
    if (load_x < 410)
        load_x = 410;
    path_x = 360;
    BTN_LOAD_X = load_x;
    BTN_LOAD_Y = 16;
    BTN_LOAD_W = 42;
    BTN_LOAD_H = 14;
    g_path_gad.LeftEdge = path_x;
    g_path_gad.TopEdge = 16;
    g_path_gad.Width = (WORD)(BTN_LOAD_X - path_x - 10);
    if (g_path_gad.Width < 80)
        g_path_gad.Width = 80;
    g_path_gad.Height = 12;

    STATUS_X = 8;
    STATUS_Y = (WORD)(win_h - status_h - 5);
    STATUS_W = (WORD)(win_w - 18);
    STATUS_TEXT_Y = (WORD)(STATUS_Y + 14);

    LOCAL_X = 10;
    LOCAL_Y = list_top;
    usable_w = (WORD)(win_w - 20 - mid_w - (gap * 2));
    pane_w = (WORD)(usable_w / 2);
    if (pane_w < 120)
        pane_w = 120;
    LOCAL_W = pane_w;
    REMOTE_X = (WORD)(LOCAL_X + LOCAL_W + mid_w + (gap * 2));
    REMOTE_W = (WORD)(win_w - REMOTE_X - 12);
    if (REMOTE_W < 120)
        REMOTE_W = 120;
    REMOTE_Y = list_top;
    LOCAL_H = (WORD)(STATUS_Y - LOCAL_Y - 8);
    if (LOCAL_H < (MIN_LIST_ROWS * ROW_H + 4))
        LOCAL_H = (WORD)(MIN_LIST_ROWS * ROW_H + 4);
    REMOTE_H = LOCAL_H;
    g_visible_rows = (LOCAL_H - 4) / ROW_H;
    if (g_visible_rows < MIN_LIST_ROWS)
        g_visible_rows = MIN_LIST_ROWS;

    BTN_UPLOAD_X = (WORD)(LOCAL_X + LOCAL_W + gap);
    BTN_UPLOAD_Y = LOCAL_Y;
    BTN_UPLOAD_W = 38;
    BTN_UPLOAD_H = 18;
    BTN_DOWNLOAD_X = BTN_UPLOAD_X;
    BTN_DOWNLOAD_Y = (WORD)(BTN_UPLOAD_Y + 28);
    BTN_DOWNLOAD_W = 38;
    BTN_DOWNLOAD_H = 18;
    BTN_DELETE_X = (WORD)(BTN_UPLOAD_X - 6);
    BTN_DELETE_Y = (WORD)(BTN_DOWNLOAD_Y + 28);
    BTN_DELETE_W = 62;
    BTN_DELETE_H = 14;
    BTN_INFO_X = BTN_DELETE_X;
    BTN_INFO_Y = (WORD)(BTN_DELETE_Y + 24);
    BTN_INFO_W = 62;
    BTN_INFO_H = 14;

    g_local_top = clamp_top(g_local_top, g_local_count);
    g_remote_top = clamp_top(g_remote_top, g_remote_count);
}

static LONG text_len(const char *s)
{
    LONG n = 0;
    if (!s)
        return 0;
    while (s[n])
        ++n;
    return n;
}

static void gui_puts(const char *text)
{
    LONG len = text_len(text);
    BPTR out = Output();

    if (out && len)
        Write(out, (APTR)text, len);
}

#if MINI_FTP_DEBUG
static void gui_put_dec(LONG value)
{
    char tmp[12];
    char rev[12];
    int i = 0;
    int j = 0;

    if (value < 0) {
        gui_puts("-");
        value = -value;
    }
    if (value == 0) {
        gui_puts("0");
        return;
    }
    while (value > 0 && j < (int)sizeof(rev)) {
        rev[j++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (j > 0)
        tmp[i++] = rev[--j];
    tmp[i] = 0;
    gui_puts(tmp);
}

static void gui_put_ip(ULONG ip)
{
    gui_put_dec((LONG)((ip >> 24) & 0xff));
    gui_puts(".");
    gui_put_dec((LONG)((ip >> 16) & 0xff));
    gui_puts(".");
    gui_put_dec((LONG)((ip >> 8) & 0xff));
    gui_puts(".");
    gui_put_dec((LONG)(ip & 0xff));
}

#define ftp_debug_puts(x) gui_puts(x)
#define ftp_debug_dec(x) gui_put_dec(x)
#define ftp_debug_ip(x) gui_put_ip(x)
#else
#define ftp_debug_puts(x) ((void)0)
#define ftp_debug_dec(x) ((void)0)
#define ftp_debug_ip(x) ((void)0)
#endif

static void gui_log_window_geometry(void)
{
    ftp_debug_puts("GUI WINDOW left=");
    ftp_debug_dec(g_new_window.LeftEdge);
    ftp_debug_puts(" top=");
    ftp_debug_dec(g_new_window.TopEdge);
    ftp_debug_puts(" width=");
    ftp_debug_dec(g_new_window.Width);
    ftp_debug_puts(" height=");
    ftp_debug_dec(g_new_window.Height);
    ftp_debug_puts(" type=");
    ftp_debug_dec(g_new_window.Type);
    ftp_debug_puts("\n");
}

static void gui_log_opened_window(void)
{
    ftp_debug_puts("GUI WINDOW opened width=");
    ftp_debug_dec(g_win ? g_win->Width : 0);
    ftp_debug_puts(" height=");
    ftp_debug_dec(g_win ? g_win->Height : 0);
    ftp_debug_puts(" depth=");
    if (g_win && g_win->WScreen)
        ftp_debug_dec(g_win->WScreen->BitMap.Depth);
    else
        ftp_debug_dec(0);
    ftp_debug_puts("\n");
}

static void refresh_string_gadgets(void)
{
    if (!g_win)
        return;
    RefreshGadgets(&g_host_gad, g_win, 0);
    RefreshGadgets(&g_port_gad, g_win, 0);
    RefreshGadgets(&g_user_gad, g_win, 0);
    RefreshGadgets(&g_path_gad, g_win, 0);
    draw_password_field();
}

static int attach_string_gadgets(void)
{
    if (!g_win)
        return 0;

    g_host_gad.NextGadget = 0;
    g_port_gad.NextGadget = 0;
    g_user_gad.NextGadget = 0;
    g_path_gad.NextGadget = 0;

    AddGadget(g_win, &g_host_gad, (ULONG)-1);
    AddGadget(g_win, &g_port_gad, (ULONG)-1);
    AddGadget(g_win, &g_user_gad, (ULONG)-1);
    AddGadget(g_win, &g_path_gad, (ULONG)-1);
    refresh_string_gadgets();
    return 1;
}

static void copy_limited(char *dst, int max_len, const char *src)
{
    int i = 0;
    if (!dst || max_len <= 0)
        return;
    if (!src)
        src = "";
    while (src[i] && i < max_len - 1) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}


static char upper_ascii(char c)
{
    if (c >= 'a' && c <= 'z')
        return (char)(c - 'a' + 'A');
    return c;
}

static int text_equal_ci(const char *a, const char *b)
{
    if (!a || !b)
        return 0;
    while (*a && *b) {
        if (upper_ascii(*a) != upper_ascii(*b))
            return 0;
        ++a;
        ++b;
    }
    return *a == 0 && *b == 0;
}

static int is_space_char(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int parse_port_field(const char *text, UWORD *out_port)
{
    ULONG value = 0;
    int digits = 0;

    if (!out_port)
        return 0;
    if (!text) {
        *out_port = FTP_PORT;
        return 1;
    }
    while (is_space_char(*text))
        ++text;
    if (!*text) {
        *out_port = FTP_PORT;
        return 1;
    }
    while (*text >= '0' && *text <= '9') {
        value = value * 10 + (ULONG)(*text - '0');
        if (value > 65535UL)
            return 0;
        ++digits;
        ++text;
    }
    while (is_space_char(*text))
        ++text;
    if (!digits || *text || value == 0)
        return 0;
    *out_port = (UWORD)value;
    return 1;
}

static void apply_tooltype_string(struct DiskObject *dobj, const char *key,
                                  char *dst, int dst_len)
{
    UBYTE *value;

    if (!dobj || !dobj->do_ToolTypes || !key || !dst)
        return;
    value = FindToolType((CONST_STRPTR *)dobj->do_ToolTypes, (CONST_STRPTR)key);
    if (value)
        copy_limited(dst, dst_len, (const char *)value);
}

static void apply_workbench_tooltypes(struct WBStartup *wbmsg, int *autoconnect)
{
    struct WBArg *arg;
    struct DiskObject *dobj;
    BPTR old_dir = 0;
    UBYTE *value;

    if (autoconnect)
        *autoconnect = 0;
    if (!wbmsg || wbmsg->sm_NumArgs <= 0 || !wbmsg->sm_ArgList)
        return;

    IconBase = OpenLibrary((CONST_STRPTR)ICONNAME, 0);
    if (!IconBase)
        return;

    arg = &wbmsg->sm_ArgList[0];
    if (arg->wa_Lock)
        old_dir = CurrentDir(arg->wa_Lock);
    dobj = GetDiskObject((CONST_STRPTR)arg->wa_Name);
    if (arg->wa_Lock)
        CurrentDir(old_dir);

    if (!dobj) {
        CloseLibrary(IconBase);
        IconBase = 0;
        return;
    }

    g_pass[0] = 0;
    apply_tooltype_string(dobj, "HOST", g_host, sizeof(g_host));
    apply_tooltype_string(dobj, "USER", g_user, sizeof(g_user));
    apply_tooltype_string(dobj, "PASSWORD", g_pass, sizeof(g_pass));
    apply_tooltype_string(dobj, "LOCALPATH", g_local_path, sizeof(g_local_path));
    apply_tooltype_string(dobj, "REMOTEPATH", g_remote_path, sizeof(g_remote_path));

    if (dobj->do_ToolTypes) {
        value = FindToolType((CONST_STRPTR *)dobj->do_ToolTypes, (CONST_STRPTR)"PORT");
        if (value)
            copy_limited(g_port, sizeof(g_port), (const char *)value);

        value = FindToolType((CONST_STRPTR *)dobj->do_ToolTypes, (CONST_STRPTR)"AUTOCONNECT");
        if (autoconnect && value && text_equal_ci((const char *)value, "YES"))
            *autoconnect = 1;
    }

    FreeDiskObject(dobj);
    CloseLibrary(IconBase);
    IconBase = 0;
}

static void append_text(char *dst, int max_len, const char *src)
{
    int pos = (int)text_len(dst);
    int i = 0;
    if (!dst || !src || max_len <= 0)
        return;
    while (src[i] && pos < max_len - 1)
        dst[pos++] = src[i++];
    dst[pos] = 0;
}

static void set_status(const char *text)
{
    copy_limited(g_status, sizeof(g_status), text);
}

static void append_status_dec(LONG value)
{
    char tmp[12];
    char rev[12];
    int i = 0;
    int j = 0;

    if (value < 0) {
        append_text(g_status, sizeof(g_status), "-");
        value = -value;
    }
    if (value == 0) {
        append_text(g_status, sizeof(g_status), "0");
        return;
    }
    while (value > 0 && j < (int)sizeof(rev)) {
        rev[j++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (j > 0)
        tmp[i++] = rev[--j];
    tmp[i] = 0;
    append_text(g_status, sizeof(g_status), tmp);
}

static void set_status_errno(const char *prefix, int err)
{
    copy_limited(g_status, sizeof(g_status), prefix);
    append_text(g_status, sizeof(g_status), " errno=");
    append_status_dec((LONG)err);
}

static void set_status_kb(const char *prefix, LONG bytes)
{
    char tmp[24];
    char rev[16];
    LONG value = bytes / 1024;
    int i = 0;
    int j = 0;

    copy_limited(g_status, sizeof(g_status), prefix);
    append_text(g_status, sizeof(g_status), " ");
    if (value == 0) {
        tmp[i++] = '0';
    } else {
        while (value > 0 && j < (int)sizeof(rev)) {
            rev[j++] = (char)('0' + (value % 10));
            value /= 10;
        }
        while (j > 0)
            tmp[i++] = rev[--j];
    }
    tmp[i] = 0;
    append_text(g_status, sizeof(g_status), tmp);
    append_text(g_status, sizeof(g_status), " KB");
}

static void draw_text_xy(WORD x, WORD y, const char *text)
{
    if (!g_win || !text)
        return;
    Move(g_win->RPort, x, y);
    Text(g_win->RPort, (STRPTR)text, text_len(text));
}

static void draw_box(WORD x, WORD y, WORD w, WORD h)
{
    Move(g_win->RPort, x, y);
    Draw(g_win->RPort, x + w, y);
    Draw(g_win->RPort, x + w, y + h);
    Draw(g_win->RPort, x, y + h);
    Draw(g_win->RPort, x, y);
}

static void draw_button_state(WORD x, WORD y, WORD w, WORD h, const char *label, int pressed)
{
    if (!g_win)
        return;
    if (pressed) {
        SetAPen(g_win->RPort, 1);
        RectFill(g_win->RPort, (WORD)(x + 1), (WORD)(y + 1),
                 (WORD)(x + w - 1), (WORD)(y + h - 1));
        SetAPen(g_win->RPort, 0);
        draw_text_xy((WORD)(x + 6), (WORD)(y + 11), label);
        SetAPen(g_win->RPort, 1);
        draw_box(x, y, w, h);
        return;
    }
    SetAPen(g_win->RPort, 0);
    RectFill(g_win->RPort, (WORD)(x + 1), (WORD)(y + 1),
             (WORD)(x + w - 1), (WORD)(y + h - 1));
    SetAPen(g_win->RPort, 1);
    draw_box(x, y, w, h);
    draw_text_xy((WORD)(x + 6), (WORD)(y + 11), label);
}

static void draw_button(WORD x, WORD y, WORD w, WORD h, const char *label)
{
    draw_button_state(x, y, w, h, label, 0);
}

static int button_at(WORD mx, WORD my)
{
    if (in_rect(mx, my, BTN_CONNECT_X, BTN_CONNECT_Y, BTN_CONNECT_W, BTN_CONNECT_H))
        return BUTTON_CONNECT;
    if (in_rect(mx, my, BTN_LOAD_X, BTN_LOAD_Y, BTN_LOAD_W, BTN_LOAD_H))
        return BUTTON_LOAD;
    if (in_rect(mx, my, BTN_UPLOAD_X, BTN_UPLOAD_Y, BTN_UPLOAD_W, BTN_UPLOAD_H))
        return BUTTON_UPLOAD;
    if (in_rect(mx, my, BTN_DOWNLOAD_X, BTN_DOWNLOAD_Y, BTN_DOWNLOAD_W, BTN_DOWNLOAD_H))
        return BUTTON_DOWNLOAD;
    if (in_rect(mx, my, BTN_DELETE_X, BTN_DELETE_Y, BTN_DELETE_W, BTN_DELETE_H))
        return BUTTON_DELETE;
    if (in_rect(mx, my, BTN_INFO_X, BTN_INFO_Y, BTN_INFO_W, BTN_INFO_H))
        return BUTTON_INFO;
    return BUTTON_NONE;
}

static void draw_button_by_id(int button, int pressed)
{
    switch (button) {
    case BUTTON_CONNECT:
        draw_button_state(BTN_CONNECT_X, BTN_CONNECT_Y, BTN_CONNECT_W, BTN_CONNECT_H, "Connect", pressed);
        break;
    case BUTTON_LOAD:
        draw_button_state(BTN_LOAD_X, BTN_LOAD_Y, BTN_LOAD_W, BTN_LOAD_H, "Load", pressed);
        break;
    case BUTTON_UPLOAD:
        draw_button_state(BTN_UPLOAD_X, BTN_UPLOAD_Y, BTN_UPLOAD_W, BTN_UPLOAD_H, "->", pressed);
        break;
    case BUTTON_DOWNLOAD:
        draw_button_state(BTN_DOWNLOAD_X, BTN_DOWNLOAD_Y, BTN_DOWNLOAD_W, BTN_DOWNLOAD_H, "<-", pressed);
        break;
    case BUTTON_DELETE:
        draw_button_state(BTN_DELETE_X, BTN_DELETE_Y, BTN_DELETE_W, BTN_DELETE_H, "Delete", pressed);
        break;
    case BUTTON_INFO:
        draw_button_state(BTN_INFO_X, BTN_INFO_Y, BTN_INFO_W, BTN_INFO_H, "Info", pressed);
        break;
    default:
        break;
    }
}

static void draw_field_frame(const struct Gadget *gad)
{
    if (!gad)
        return;
    draw_box((WORD)(gad->LeftEdge - 2),
             (WORD)(gad->TopEdge - 2),
             (WORD)(gad->Width + 3),
             (WORD)(gad->Height + 3));
}

static int text_equal(const char *a, const char *b)
{
    if (!a || !b)
        return 0;
    while (*a && *b) {
        if (*a != *b)
            return 0;
        ++a;
        ++b;
    }
    return *a == 0 && *b == 0;
}

static int text_compare_ci(const char *a, const char *b)
{
    char ca;
    char cb;

    if (!a)
        a = "";
    if (!b)
        b = "";
    while (*a && *b) {
        ca = upper_ascii(*a);
        cb = upper_ascii(*b);
        if (ca != cb)
            return (int)((UBYTE)ca) - (int)((UBYTE)cb);
        ++a;
        ++b;
    }
    return (int)((UBYTE)upper_ascii(*a)) - (int)((UBYTE)upper_ascii(*b));
}

static int ftp_entry_before(const struct FtpEntry *a, const struct FtpEntry *b)
{
    int cmp;

    if (a->is_dir != b->is_dir)
        return a->is_dir != 0;
    cmp = text_compare_ci(a->name, b->name);
    if (cmp != 0)
        return cmp < 0;
    return 0;
}

static void sort_entries(struct FtpEntry *entries, int count)
{
    int start = 0;
    int i;

    if (!entries || count <= 1)
        return;
    if (text_equal(entries[0].name, ".."))
        start = 1;
    for (i = start + 1; i < count; ++i) {
        struct FtpEntry item = entries[i];
        int j = i;
        while (j > start && ftp_entry_before(&item, &entries[j - 1])) {
            entries[j] = entries[j - 1];
            --j;
        }
        entries[j] = item;
    }
}

static void draw_password_field(void)
{
    int i;
    int len = (int)text_len(g_pass);
    char stars[PASS_BUF_SIZE + 2];

    if (!g_win)
        return;

    draw_box((WORD)(PASS_FIELD_X - 2),
             (WORD)(PASS_FIELD_Y - 2),
             (WORD)(PASS_FIELD_W + 3),
             (WORD)(PASS_FIELD_H + 3));
    clear_rect(PASS_FIELD_X, PASS_FIELD_Y, PASS_FIELD_W, PASS_FIELD_H);

    if (len > PASS_BUF_SIZE - 2)
        len = PASS_BUF_SIZE - 2;
    for (i = 0; i < len; ++i)
        stars[i] = '*';
    if (g_pass_active && len < PASS_BUF_SIZE - 1)
        stars[len++] = '_';
    stars[len] = 0;
    draw_text_xy((WORD)(PASS_FIELD_X + 2), (WORD)(PASS_FIELD_Y + 10), stars);
}

static void clear_rect(WORD x, WORD y, WORD w, WORD h)
{
    SetAPen(g_win->RPort, 0);
    RectFill(g_win->RPort, x, y, (WORD)(x + w), (WORD)(y + h));
    SetAPen(g_win->RPort, 1);
}

static int clamp_top(int top, int count)
{
    int max_top = count - g_visible_rows;

    if (max_top < 0)
        max_top = 0;
    if (top < 0)
        return 0;
    if (top > max_top)
        return max_top;
    return top;
}

static void draw_scrollbar(WORD x, WORD y, WORD h, int count, int top)
{
    WORD thumb_h;
    WORD thumb_y;
    WORD track_h = (WORD)(h - 4);
    int max_top = count - g_visible_rows;

    draw_box(x, y, SCROLL_W, h);
    if (count <= g_visible_rows || track_h <= 0)
        return;

    thumb_h = (WORD)((g_visible_rows * track_h) / count);
    if (thumb_h < 8)
        thumb_h = 8;
    if (thumb_h > track_h)
        thumb_h = track_h;

    if (max_top <= 0)
        thumb_y = (WORD)(y + 2);
    else
        thumb_y = (WORD)(y + 2 + ((top * (track_h - thumb_h)) / max_top));

    RectFill(g_win->RPort, (WORD)(x + 2), thumb_y, (WORD)(x + SCROLL_W - 2), (WORD)(thumb_y + thumb_h));
}

static void draw_list(WORD x, WORD y, WORD w, WORD h,
                      struct FtpEntry *entries, int count, int selected,
                      int top, int show_dirs)
{
    int i;
    int max = g_visible_rows;
    WORD text_w = (WORD)(w - SCROLL_W - 3);
    char display[NAME_BUF_SIZE + 8];

    top = clamp_top(top, count);
    clear_rect(x + 1, y + 1, (WORD)(w - 2), (WORD)(h - 2));
    draw_box(x, y, w, h);
    draw_scrollbar((WORD)(x + w - SCROLL_W - 1), (WORD)(y + 1), (WORD)(h - 2), count, top);
    if (count - top < max)
        max = count - top;
    for (i = 0; i < max; ++i) {
        WORD row_y = (WORD)(y + 10 + i * ROW_H);
        int entry_index = top + i;
        if (entry_index == selected) {
            SetAPen(g_win->RPort, 1);
            RectFill(g_win->RPort, (WORD)(x + 2), (WORD)(row_y - 7), (WORD)(x + text_w), (WORD)(row_y + 1));
            SetAPen(g_win->RPort, 0);
        }
        if (show_dirs && entries[entry_index].is_dir &&
            !text_equal(entries[entry_index].name, "..")) {
            copy_limited(display, sizeof(display), "[DIR] ");
            append_text(display, sizeof(display), entries[entry_index].name);
            draw_text_xy((WORD)(x + 4), row_y, display);
        } else {
            draw_text_xy((WORD)(x + 4), row_y, entries[entry_index].name);
        }
        SetAPen(g_win->RPort, 1);
    }
}

static void draw_ui(void)
{
    WORD win_w;
    WORD win_h;

    if (!g_win)
        return;
    update_layout();
    win_w = g_win->Width;
    win_h = g_win->Height;
    SetDrMd(g_win->RPort, JAM1);
    SetAPen(g_win->RPort, 0);
    RectFill(g_win->RPort, 0, 10, (WORD)(win_w - 1), (WORD)(win_h - 1));
    SetAPen(g_win->RPort, 1);

    draw_box(4, 13, (WORD)(win_w - 10), (WORD)(win_h - 19));

    draw_text_xy(12, 25, "Host:");
    draw_text_xy((WORD)(g_port_gad.LeftEdge - 40), 25, "Port:");
    draw_text_xy(12, 41, "User:");
    draw_text_xy(12, 57, "Pass:");
    draw_field_frame(&g_host_gad);
    draw_field_frame(&g_port_gad);
    draw_field_frame(&g_user_gad);
    draw_password_field();
    draw_button(BTN_CONNECT_X, BTN_CONNECT_Y, BTN_CONNECT_W, BTN_CONNECT_H, "Connect");

    draw_text_xy((WORD)(g_path_gad.LeftEdge - 55), 25, "Local:");
    draw_field_frame(&g_path_gad);
    draw_button(BTN_LOAD_X, BTN_LOAD_Y, BTN_LOAD_W, BTN_LOAD_H, "Load");
    draw_text_xy(230, 45, "Remote:");
    draw_text_xy(285, 45, g_remote_path);

    draw_text_xy(LOCAL_X, (WORD)(LOCAL_Y - 4), "Local files");
    draw_text_xy(REMOTE_X, (WORD)(REMOTE_Y - 4), "FTP files");
    draw_list(LOCAL_X, LOCAL_Y, LOCAL_W, LOCAL_H, g_local_entries, g_local_count, g_local_sel, g_local_top, 1);
    draw_list(REMOTE_X, REMOTE_Y, REMOTE_W, REMOTE_H, g_remote_entries, g_remote_count, g_remote_sel, g_remote_top, 1);
    draw_button(BTN_UPLOAD_X, BTN_UPLOAD_Y, BTN_UPLOAD_W, BTN_UPLOAD_H, "->");
    draw_button(BTN_DOWNLOAD_X, BTN_DOWNLOAD_Y, BTN_DOWNLOAD_W, BTN_DOWNLOAD_H, "<-");
    draw_button(BTN_DELETE_X, BTN_DELETE_Y, BTN_DELETE_W, BTN_DELETE_H, "Delete");
    draw_button(BTN_INFO_X, BTN_INFO_Y, BTN_INFO_W, BTN_INFO_H, "Info");

    clear_rect((WORD)(STATUS_X + 2), (WORD)(STATUS_Y + 3), (WORD)(STATUS_W - 4), 12);
    draw_box(STATUS_X, STATUS_Y, STATUS_W, 17);
    draw_text_xy((WORD)(STATUS_X + 6), STATUS_TEXT_Y, g_status);
    refresh_string_gadgets();
}

static void draw_status_now(void)
{
    if (!g_win)
        return;
    update_layout();
    clear_rect((WORD)(STATUS_X + 2), (WORD)(STATUS_Y + 3), (WORD)(STATUS_W - 4), 12);
    draw_box(STATUS_X, STATUS_Y, STATUS_W, 17);
    draw_text_xy((WORD)(STATUS_X + 6), STATUS_TEXT_Y, g_status);
}

static void set_status_draw(const char *text)
{
    set_status(text);
    draw_status_now();
}

static int in_rect(WORD mx, WORD my, WORD x, WORD y, WORD w, WORD h)
{
    return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

static int list_scrollbar_hit(WORD mx, WORD my, WORD x, WORD y, WORD w, WORD h)
{
    return in_rect(mx, my, (WORD)(x + w - SCROLL_W - 1), (WORD)(y + 1), SCROLL_W, (WORD)(h - 2));
}

static int top_from_scroll_y(WORD my, WORD y, WORD h, int count)
{
    int max_top = count - g_visible_rows;
    int track_h = h - 4;
    int rel = my - y - 2;

    if (max_top <= 0 || track_h <= 0)
        return 0;
    if (rel < 0)
        rel = 0;
    if (rel > track_h)
        rel = track_h;
    return (rel * max_top) / track_h;
}

static void update_scroll_from_mouse(int which, WORD my)
{
    if (which == SCROLL_LOCAL) {
        g_local_top = clamp_top(top_from_scroll_y(my, LOCAL_Y, LOCAL_H, g_local_count), g_local_count);
        draw_ui();
    } else if (which == SCROLL_REMOTE) {
        g_remote_top = clamp_top(top_from_scroll_y(my, REMOTE_Y, REMOTE_H, g_remote_count), g_remote_count);
        draw_ui();
    }
}

static const char *basename_any(const char *path)
{
    const char *base = path;
    const char *p = path;
    if (!path)
        return 0;
    while (*p) {
        if (*p == '/' || *p == ':')
            base = p + 1;
        ++p;
    }
    return *base ? base : 0;
}

static void build_local_full_path(char *out, int out_len, const char *name)
{
    copy_limited(out, out_len, g_local_path);
    if (text_len(out) > 0 && out[text_len(out) - 1] != ':' && out[text_len(out) - 1] != '/')
        append_text(out, out_len, "/");
    append_text(out, out_len, name);
}

static void add_local_parent_entry(void)
{
    if (g_local_count >= MAX_LOCAL_ENTRIES)
        return;
    copy_limited(g_local_entries[g_local_count].name, NAME_BUF_SIZE, "..");
    g_local_entries[g_local_count].is_dir = 1;
    ++g_local_count;
}

static void local_path_enter(const char *dir)
{
    if (!dir || !dir[0] || text_equal(dir, ".."))
        return;
    if (text_len(g_local_path) > 0 &&
        g_local_path[text_len(g_local_path) - 1] != ':' &&
        g_local_path[text_len(g_local_path) - 1] != '/')
        append_text(g_local_path, sizeof(g_local_path), "/");
    append_text(g_local_path, sizeof(g_local_path), dir);
}

static int local_path_parent(void)
{
    LONG len = text_len(g_local_path);
    LONG i;

    if (len <= 0)
        return 0;

    while (len > 0 && g_local_path[len - 1] == '/')
        g_local_path[--len] = 0;

    if (len <= 0)
        return 0;

    if (g_local_path[len - 1] == ':')
        return 0;

    i = len - 1;
    while (i >= 0 && g_local_path[i] != '/' && g_local_path[i] != ':')
        --i;

    if (i < 0)
        return 0;

    if (g_local_path[i] == ':') {
        g_local_path[i + 1] = 0;
    } else {
        while (i > 0 && g_local_path[i - 1] == '/')
            --i;
        if (i <= 0)
            return 0;
        g_local_path[i] = 0;
    }

    return 1;
}

static int parse_dec_octet(const char **p, UBYTE *out)
{
    ULONG value = 0;
    int digits = 0;
    const char *s = *p;
    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (ULONG)(*s - '0');
        if (value > 255)
            return 0;
        ++digits;
        ++s;
    }
    if (!digits)
        return 0;
    *out = (UBYTE)value;
    *p = s;
    return 1;
}

static int parse_ipv4(const char *text, ULONG *out_ip)
{
    UBYTE parts[4];
    const char *p = text;
    int i;
    for (i = 0; i < 4; ++i) {
        if (!parse_dec_octet(&p, &parts[i]))
            return 0;
        if (i != 3 && *p++ != '.')
            return 0;
    }
    if (*p)
        return 0;
    *out_ip = ((ULONG)parts[0] << 24) | ((ULONG)parts[1] << 16) |
              ((ULONG)parts[2] << 8) | (ULONG)parts[3];
    return 1;
}

static int call_socket(struct Library *base, int domain, int type, int protocol)
{
    register int d0 __asm("d0") = domain;
    register int d1 __asm("d1") = type;
    register int d2 __asm("d2") = protocol;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-30:W)" : "+r" (d0), "+r" (d1), "+r" (d2) : "r" (a6) : "a0", "a1", "cc", "memory");
    return d0;
}

static int call_close_socket(struct Library *base, int fd)
{
    register int d0 __asm("d0") = fd;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-120:W)" : "+r" (d0) : "r" (a6) : "d1", "a0", "a1", "cc", "memory");
    return d0;
}

static int call_connect(struct Library *base, int fd, const struct Amitcp13BsdSockAddr *addr, int addrlen)
{
    register int d0 __asm("d0") = fd;
    register const struct Amitcp13BsdSockAddr *a0 __asm("a0") = addr;
    register int d1 __asm("d1") = addrlen;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-54:W)" : "+r" (d0), "+r" (a0), "+r" (d1) : "r" (a6) : "a1", "cc", "memory");
    return d0;
}

static int call_send(struct Library *base, int fd, const void *buf, int len, int flags)
{
    register int d0 __asm("d0") = fd;
    register const void *a0 __asm("a0") = buf;
    register int d1 __asm("d1") = len;
    register int d2 __asm("d2") = flags;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-66:W)" : "+r" (d0), "+r" (a0), "+r" (d1), "+r" (d2) : "r" (a6) : "a1", "cc", "memory");
    return d0;
}

static int call_recv(struct Library *base, int fd, void *buf, int len, int flags)
{
    register int d0 __asm("d0") = fd;
    register void *a0 __asm("a0") = buf;
    register int d1 __asm("d1") = len;
    register int d2 __asm("d2") = flags;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-78:W)" : "+r" (d0), "+r" (a0), "+r" (d1), "+r" (d2) : "r" (a6) : "a1", "cc", "memory");
    return d0;
}

static int call_ioctl(struct Library *base, int fd, ULONG request, void *argp)
{
    register int d0 __asm("d0") = fd;
    register ULONG d1 __asm("d1") = request;
    register void *a0 __asm("a0") = argp;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-114:W)" : "+r" (d0), "+r" (d1), "+r" (a0) : "r" (a6) : "a1", "cc", "memory");
    return d0;
}

static int call_getsockopt(struct Library *base, int fd, int level, int optname, void *optval, int *optlen)
{
    register int d0 __asm("d0") = fd;
    register int d1 __asm("d1") = level;
    register int d2 __asm("d2") = optname;
    register void *a0 __asm("a0") = optval;
    register int *a1 __asm("a1") = optlen;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-96:W)" : "+r" (d0), "+r" (d1), "+r" (d2), "+r" (a0), "+r" (a1) : "r" (a6) : "cc", "memory");
    return d0;
}

static int call_waitselect(struct Library *base, int nfds, struct Amitcp13BsdFdSet *readfds, struct Amitcp13BsdFdSet *writefds, const struct Amitcp13BsdTimeVal *timeout)
{
    register int d0 __asm("d0") = nfds;
    register ULONG *d1 __asm("d1") = &g_wait_signals;
    register struct Amitcp13BsdFdSet *a0 __asm("a0") = readfds;
    register struct Amitcp13BsdFdSet *a1 __asm("a1") = writefds;
    register struct Amitcp13BsdFdSet *a2 __asm("a2") = 0;
    register const struct Amitcp13BsdTimeVal *a3 __asm("a3") = timeout;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-126:W)" : "+r" (d0), "+r" (d1), "+r" (a0), "+r" (a1), "+r" (a2), "+r" (a3) : "r" (a6) : "cc", "memory");
    return d0;
}

static int call_errno(struct Library *base)
{
    register int d0 __asm("d0");
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-162:W)" : "=r" (d0) : "r" (a6) : "d1", "a0", "a1", "cc", "memory");
    return d0;
}

static struct hostent *call_gethostbyname(struct Library *base, const char *name)
{
    register const char *a0 __asm("a0") = name;
    register struct Library *a6 __asm("a6") = base;
    register struct hostent *d0 __asm("d0");
    __asm volatile ("jsr a6@(-210:W)" : "=r" (d0), "+r" (a0) : "r" (a6) : "d1", "a1", "cc", "memory");
    return d0;
}

static int wait_for_socket(struct Library *base, int fd, int want_write)
{
    int result;
    AMITCP13_BSD_FD_ZERO(&g_read_fds);
    AMITCP13_BSD_FD_ZERO(&g_write_fds);
    if (want_write)
        AMITCP13_BSD_FD_SET(fd, &g_write_fds);
    else
        AMITCP13_BSD_FD_SET(fd, &g_read_fds);
    g_timeout.tv_sec = TIMEOUT_SECONDS;
    g_timeout.tv_usec = 0;
    g_wait_signals = 0;
    result = call_waitselect(base, fd + 1, want_write ? 0 : &g_read_fds, want_write ? &g_write_fds : 0, &g_timeout);
    if (result > 0)
        return 1;
    if (result == 0)
        set_status("Timeout waiting for server");
    else
        set_status_errno("WaitSelect failed", call_errno(base));
    draw_status_now();
    return 0;
}

static int socket_would_block_error(int err)
{
    return err == AMITCP13_EWOULDBLOCK ||
           err == AMITCP13_EAGAIN ||
           err == BSD_EAGAIN_COMPAT;
}

static int send_all(struct Library *base, int fd, const char *buf, int len)
{
    int sent_total = 0;
    int sent;
    int err;
    int retries = 0;
    while (sent_total < len) {
        if (!wait_for_socket(base, fd, 1))
            return 0;
        sent = call_send(base, fd, buf + sent_total, len - sent_total, 0);
        if (sent > 0) {
            sent_total += sent;
            retries = 0;
            continue;
        }
        if (sent == 0) {
            if (++retries > CONTROL_SEND_RETRY_LIMIT) {
                set_status("send failed");
                draw_status_now();
                return 0;
            }
            continue;
        }
        err = call_errno(base);
        if (socket_would_block_error(err) ||
            err == AMITCP13_EINTR) {
            if (++retries > CONTROL_SEND_RETRY_LIMIT) {
                set_status("send failed");
                draw_status_now();
                return 0;
            }
            continue;
        }
        set_status_errno("send failed", err);
        draw_status_now();
        return 0;
    }
    return 1;
}

static int socket_retry_error(int err)
{
    return socket_would_block_error(err) ||
           err == AMITCP13_EINTR ||
           err == AMITCP13_EIO ||
           err == AMITCP13_ENOBUFS;
}

static void upload_drain_socket(struct Library *base, int fd)
{
    int i;
    int got;
    int err;

    for (i = 0; i < UPLOAD_DRAIN_POLLS; ++i) {
        if (!wait_for_socket(base, fd, 1))
            return;
        got = call_recv(base, fd, g_data_buf, sizeof(g_data_buf), 0);
        if (got == 0)
            return;
        if (got < 0) {
            err = call_errno(base);
            if (socket_retry_error(err))
                continue;
            return;
        }
    }
}

static int upload_send_chunk(struct Library *base, int fd, const char *buf, int len, LONG *total)
{
    int sent_total = 0;
    int sent;
    int err;
    int retries = 0;
    while (sent_total < len) {
        if (!wait_for_socket(base, fd, 1))
            return 0;
        sent = call_send(base, fd, buf + sent_total, len - sent_total, 0);
        if (sent_total == 0 && total && *total == 0) {
            ftp_debug_puts("FTP GUI PUT first Send() returned=");
            ftp_debug_dec((LONG)sent);
            if (sent < 0) {
                ftp_debug_puts(" errno=");
                ftp_debug_dec((LONG)call_errno(base));
            }
            ftp_debug_puts("\n");
        }
        if (sent > 0) {
            sent_total += sent;
            if (total)
                *total += sent;
            retries = 0;
            continue;
        }
        if (sent == 0) {
            if (++retries > UPLOAD_RETRY_LIMIT) {
                set_status("send failed");
                draw_status_now();
                return 0;
            }
            continue;
        }
        err = call_errno(base);
        if (socket_retry_error(err)) {
            if (++retries > UPLOAD_RETRY_LIMIT) {
                set_status("send failed");
                draw_status_now();
                return 0;
            }
            continue;
        }
        set_status_errno("send failed", err);
        draw_status_now();
        return 0;
    }
    return 1;
}

static int make_nonblocking(struct Library *base, int fd)
{
    g_one = 1;
    return call_ioctl(base, fd, FIONBIO, &g_one) == 0;
}

static int resolve_host(struct Library *base, const char *host, ULONG *out_ip)
{
    struct hostent *he;
    if (parse_ipv4(host, out_ip))
        return 1;
    he = call_gethostbyname(base, host);
    if (!he || !he->h_addr_list || !he->h_addr_list[0])
        return 0;
    *out_ip = *(ULONG *)he->h_addr_list[0];
    return 1;
}

static int connect_fd(struct Library *base, int fd, ULONG ip, UWORD port)
{
    int result;
    int err;
    ftp_debug_puts("FTP GUI connect fd=");
    ftp_debug_dec((LONG)fd);
    ftp_debug_puts(" endpoint=");
    ftp_debug_ip(ip);
    ftp_debug_puts(":");
    ftp_debug_dec((LONG)port);
    ftp_debug_puts("\n");
    g_addr.sin_len = sizeof(g_addr);
    g_addr.sin_family = AMITCP13_AF_INET;
    g_addr.sin_port = port;
    g_addr.sin_addr.s_addr = ip;
    result = call_connect(base, fd, (const struct Amitcp13BsdSockAddr *)&g_addr, sizeof(g_addr));
    if (result == 0)
        return 1;
    err = call_errno(base);
    ftp_debug_puts("FTP GUI Connect() initial errno=");
    ftp_debug_dec((LONG)err);
    ftp_debug_puts("\n");
    if (err != AMITCP13_EINPROGRESS && err != AMITCP13_EALREADY) {
        set_status_errno("Connect failed", err);
        draw_status_now();
        return 0;
    }
    if (!wait_for_socket(base, fd, 1))
        return 0;
    ftp_debug_puts("FTP GUI WaitSelect connect writable fd=");
    ftp_debug_dec((LONG)fd);
    ftp_debug_puts("\n");
    g_so_error = -1;
    g_so_error_len = sizeof(g_so_error);
    if (call_getsockopt(base, fd, SOL_SOCKET, SO_ERROR, &g_so_error, &g_so_error_len) < 0) {
        set_status_errno("SO_ERROR failed", call_errno(base));
        draw_status_now();
        return 0;
    }
    if (g_so_error != 0) {
        set_status_errno("Connect failed", g_so_error);
        draw_status_now();
        return 0;
    }
    return 1;
}

static int open_connected_socket(struct Library *base, ULONG ip, UWORD port)
{
    int fd = call_socket(base, AMITCP13_AF_INET, AMITCP13_SOCK_STREAM, AMITCP13_IPPROTO_TCP);
    if (fd < 0) {
        set_status_errno("Socket failed", call_errno(base));
        draw_status_now();
        return -1;
    }
    ftp_debug_puts("FTP GUI socket created fd=");
    ftp_debug_dec((LONG)fd);
    ftp_debug_puts("\n");
    if (!make_nonblocking(base, fd)) {
        set_status_errno("Ioctl failed", call_errno(base));
        draw_status_now();
        call_close_socket(base, fd);
        return -1;
    }
    if (!connect_fd(base, fd, ip, port)) {
        call_close_socket(base, fd);
        return -1;
    }
    return fd;
}

static int read_line(struct Library *base, int fd, char *line, int max_len)
{
    int pos = 0;
    char ch;
    int got;
    int err;
    for (;;) {
        if (!wait_for_socket(base, fd, 0))
            return 0;
        got = call_recv(base, fd, &ch, 1, 0);
        if (got == 1) {
            if (ch == '\r')
                continue;
            if (ch == '\n') {
                line[pos] = 0;
                return 1;
            }
            if (pos < max_len - 1)
                line[pos++] = ch;
            continue;
        }
        if (got == 0) {
            line[pos] = 0;
            if (pos == 0) {
                set_status("Connection closed");
                draw_status_now();
            }
            return pos > 0;
        }
        err = call_errno(base);
        if (socket_would_block_error(err) ||
            err == AMITCP13_EINTR) {
            continue;
        }
        set_status_errno("recv failed", err);
        draw_status_now();
        return 0;
    }
}

static int line_code(const char *line)
{
    if (!line || line[0] < '0' || line[0] > '9' || line[1] < '0' ||
        line[1] > '9' || line[2] < '0' || line[2] > '9')
        return 0;
    return (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
}

static int read_response(struct Library *base, int fd, int *out_code)
{
    int code;
    int first_code = 0;
    int multiline = 0;
    for (;;) {
        if (!read_line(base, fd, g_line, LINE_BUF_SIZE)) {
            return 0;
        }
        ftp_debug_puts("FTP GUI reply: ");
        ftp_debug_puts(g_line);
        ftp_debug_puts("\n");
        code = line_code(g_line);
        if (!code)
            continue;
        if (!first_code) {
            first_code = code;
            multiline = (g_line[3] == '-');
            if (!multiline)
                break;
        } else if (code == first_code && g_line[3] == ' ') {
            break;
        }
    }
    if (out_code)
        *out_code = first_code;
    return 1;
}

static int build_command1(const char *cmd, const char *arg)
{
    int pos = 0;
    g_cmd[0] = 0;
    append_text(g_cmd, CMD_BUF_SIZE, cmd);
    if (arg && arg[0]) {
        append_text(g_cmd, CMD_BUF_SIZE, " ");
        append_text(g_cmd, CMD_BUF_SIZE, arg);
    }
    append_text(g_cmd, CMD_BUF_SIZE, "\r\n");
    pos = (int)text_len(g_cmd);
    return pos < CMD_BUF_SIZE - 1 ? pos : 0;
}

static int ftp_command(struct Library *base, int fd, const char *cmd, const char *arg, int *out_code)
{
    int len = build_command1(cmd, arg);
    if (len <= 0)
        return 0;
    ftp_debug_puts("FTP GUI command: ");
    ftp_debug_puts(cmd);
    if (arg && arg[0]) {
        ftp_debug_puts(" ");
        if (text_equal(cmd, "PASS"))
            ftp_debug_puts("***");
        else
            ftp_debug_puts(arg);
    }
    ftp_debug_puts("\n");
    if (!send_all(base, fd, g_cmd, len))
        return 0;
    return read_response(base, fd, out_code);
}

static int parse_pasv_number(const char **p, UWORD *out)
{
    ULONG value = 0;
    int digits = 0;
    while (**p >= '0' && **p <= '9') {
        value = value * 10 + (ULONG)(**p - '0');
        if (value > 255)
            return 0;
        ++digits;
        ++*p;
    }
    if (!digits)
        return 0;
    *out = (UWORD)value;
    return 1;
}

static int parse_pasv(ULONG *out_ip, UWORD *out_port)
{
    const char *p = g_line;
    UWORD v[6];
    int i;
    while (*p && *p != '(')
        ++p;
    if (*p != '(')
        return 0;
    ++p;
    for (i = 0; i < 6; ++i) {
        if (!parse_pasv_number(&p, &v[i]))
            return 0;
        if (i != 5 && *p++ != ',')
            return 0;
    }
    *out_ip = ((ULONG)v[0] << 24) | ((ULONG)v[1] << 16) |
              ((ULONG)v[2] << 8) | (ULONG)v[3];
    *out_port = (UWORD)(v[4] * 256 + v[5]);
    return *out_port != 0;
}

static int enter_pasv(struct Library *base, int ctrl_fd, ULONG *data_ip, UWORD *data_port)
{
    int code;
    if (!ftp_command(base, ctrl_fd, "PASV", 0, &code) || code != 227)
        return 0;
    if (!parse_pasv(data_ip, data_port))
        return 0;
    ftp_debug_puts("FTP GUI PASV raw: ");
    ftp_debug_puts(g_line);
    ftp_debug_puts("\n");
    ftp_debug_puts("FTP GUI PASV parsed endpoint=");
    ftp_debug_ip(*data_ip);
    ftp_debug_puts(":");
    ftp_debug_dec((LONG)*data_port);
    ftp_debug_puts("\n");
    return 1;
}

static int status_is_timeout(void)
{
    return text_equal(g_status, "Timeout waiting for server");
}

static void clear_remote_session_state(void)
{
    g_connected = 0;
    g_remote_count = 0;
    g_remote_sel = -1;
    g_remote_top = 0;
    g_last_remote_click_index = -1;
    copy_limited(g_remote_path, sizeof(g_remote_path), "/");
}

static int ftp_control_is_connected(void)
{
    return g_sock_base && g_ctrl_fd >= 0 && g_connected;
}

static void close_data_socket(int *fd)
{
    if (fd && *fd >= 0 && g_sock_base) {
        ftp_debug_puts("FTP GUI data fd close=");
        ftp_debug_dec((LONG)*fd);
        ftp_debug_puts("\n");
        call_close_socket(g_sock_base, *fd);
        if (*fd == g_data_fd)
            g_data_fd = -1;
    }
    if (fd)
        *fd = -1;
}

static void ftp_close_data(void)
{
    close_data_socket(&g_data_fd);
}

static void ftp_close_control(void)
{
    ftp_close_data();
    if (g_sock_base && g_ctrl_fd >= 0) {
        ftp_debug_puts("FTP GUI control fd close=");
        ftp_debug_dec((LONG)g_ctrl_fd);
        ftp_debug_puts("\n");
        call_close_socket(g_sock_base, g_ctrl_fd);
    }
    g_ctrl_fd = -1;
    g_connected = 0;
}

static void ftp_gui_disconnect_session(const char *status)
{
    ftp_close_control();
    clear_remote_session_state();
    if (status && status[0])
        set_status(status);
    if (g_win)
        draw_ui();
}

static int ftp_pasv_open_data(const char *status_on_fail)
{
    ULONG data_ip;
    UWORD data_port;
    int fd;

    ftp_close_data();
    if (!ftp_control_is_connected()) {
        set_status_draw("Not connected");
        return -1;
    }
    if (!enter_pasv(g_sock_base, g_ctrl_fd, &data_ip, &data_port)) {
        if (status_on_fail && status_on_fail[0])
            set_status_draw(status_on_fail);
        return -1;
    }
    set_status_draw("Connecting data socket...");
    fd = open_connected_socket(g_sock_base, data_ip, data_port);
    if (fd < 0) {
        ftp_close_data();
        if (g_status[0] == 0 && status_on_fail && status_on_fail[0])
            set_status_draw(status_on_fail);
        return -1;
    }
    g_data_fd = fd;
    ftp_debug_puts("FTP GUI data fd open=");
    ftp_debug_dec((LONG)g_data_fd);
    ftp_debug_puts("\n");
    return g_data_fd;
}

static int ftp_read_final_transfer_reply(const char *status_on_fail)
{
    int code;

    if (read_response(g_sock_base, g_ctrl_fd, &code) &&
        (code == 226 || code == 250))
        return 1;
    if (status_on_fail && status_on_fail[0])
        set_status_draw(status_on_fail);
    return 0;
}

static int parse_size_response(LONG *out_size)
{
    const char *p = g_line;
    LONG value = 0;
    int digits = 0;

    if (!out_size)
        return 0;
    *out_size = -1;
    while (*p && *p != ' ')
        ++p;
    while (*p == ' ')
        ++p;
    while (*p >= '0' && *p <= '9') {
        value = value * 10 + (LONG)(*p - '0');
        if (value < 0)
            return 0;
        ++digits;
        ++p;
    }
    if (!digits)
        return 0;
    *out_size = value;
    return 1;
}

static int ftp_get_remote_size(const char *remote, LONG *out_size)
{
    int code;

    if (out_size)
        *out_size = -1;
    if (!remote || !remote[0])
        return 1;
    if (!ftp_command(g_sock_base, g_ctrl_fd, "SIZE", remote, &code))
        return 0;
    if (code == 213)
        return parse_size_response(out_size);
    return 1;
}

static void ftp_recover_control_after_transfer_error(const char *status)
{
    ftp_close_data();
    ftp_gui_disconnect_session(status);
}

static void add_remote_line(const char *line)
{
    const char *name = line;
    const char *p = line;
    int spaces = 0;
    if (g_remote_count >= MAX_REMOTE_ENTRIES || !line || !*line)
        return;
    while (*p) {
        if (*p == ' ') {
            while (*p == ' ')
                ++p;
            ++spaces;
            if (spaces >= 8) {
                name = p;
                break;
            }
        } else {
            ++p;
        }
    }
    if (text_equal(name, ".") || text_equal(name, ".."))
        return;
    copy_limited(g_remote_entries[g_remote_count].name, NAME_BUF_SIZE, name);
    g_remote_entries[g_remote_count].is_dir = (line[0] == 'd');
    ++g_remote_count;
}

static void add_remote_parent_entry(void)
{
    if (g_remote_count >= MAX_REMOTE_ENTRIES)
        return;
    copy_limited(g_remote_entries[g_remote_count].name, NAME_BUF_SIZE, "..");
    g_remote_entries[g_remote_count].is_dir = 1;
    ++g_remote_count;
}

static void remote_path_enter(const char *dir)
{
    if (!dir || !dir[0] || text_equal(dir, ".") || text_equal(dir, ".."))
        return;
    if (!g_remote_path[0])
        copy_limited(g_remote_path, sizeof(g_remote_path), "/");
    if (!text_equal(g_remote_path, "/") &&
        g_remote_path[text_len(g_remote_path) - 1] != '/')
        append_text(g_remote_path, sizeof(g_remote_path), "/");
    append_text(g_remote_path, sizeof(g_remote_path), dir);
}

static void remote_path_parent(void)
{
    LONG len = text_len(g_remote_path);
    LONG i;

    if (len <= 1) {
        copy_limited(g_remote_path, sizeof(g_remote_path), "/");
        return;
    }
    if (g_remote_path[len - 1] == '/' && len > 1)
        g_remote_path[--len] = 0;
    i = len - 1;
    while (i > 0 && g_remote_path[i] != '/')
        --i;
    if (i <= 0) {
        copy_limited(g_remote_path, sizeof(g_remote_path), "/");
    } else {
        g_remote_path[i] = 0;
    }
}

static int ftp_list_remote(void)
{
    int data_fd = -1;
    int code;
    int got;
    int err;
    int line_pos = 0;

    if (!g_connected)
        return 0;
    ftp_debug_puts("FTP GUI LIST start\n");
    set_status_draw("Listing remote...");
    data_fd = ftp_pasv_open_data("LIST failed");
    if (data_fd < 0) {
        ftp_gui_disconnect_session("LIST failed: reconnect required");
        return 0;
    }
    if (!ftp_command(g_sock_base, g_ctrl_fd, "LIST", 0, &code) || (code != 125 && code != 150)) {
        close_data_socket(&data_fd);
        set_status_draw("LIST failed");
        return 0;
    }
    g_remote_count = 0;
    g_remote_sel = -1;
    g_remote_top = 0;
    add_remote_parent_entry();
    g_line[0] = 0;
    for (;;) {
        if (!wait_for_socket(g_sock_base, data_fd, 0)) {
            close_data_socket(&data_fd);
            ftp_recover_control_after_transfer_error("LIST failed: reconnect required");
            return 0;
        }
        for (;;) {
            got = call_recv(g_sock_base, data_fd, g_data_buf, sizeof(g_data_buf), 0);
            if (got > 0) {
                int i;
                for (i = 0; i < got; ++i) {
                    char ch = (char)g_data_buf[i];
                    if (ch == '\r')
                        continue;
                    if (ch == '\n') {
                        g_line[line_pos] = 0;
                        add_remote_line(g_line);
                        line_pos = 0;
                    } else if (line_pos < LINE_BUF_SIZE - 1) {
                        g_line[line_pos++] = ch;
                    }
                }
                continue;
            }
            if (got == 0) {
                if (line_pos > 0) {
                    g_line[line_pos] = 0;
                    add_remote_line(g_line);
                }
                close_data_socket(&data_fd);
                if (!ftp_read_final_transfer_reply("LIST failed")) {
                    ftp_gui_disconnect_session("LIST failed: reconnect required");
                    return 0;
                }
                sort_entries(g_remote_entries, g_remote_count);
                ftp_debug_puts("FTP GUI LIST end\n");
                set_status("Remote list loaded");
                draw_ui();
                return 1;
            }
            err = call_errno(g_sock_base);
            if (socket_retry_error(err))
                break;
            close_data_socket(&data_fd);
            set_status_errno("remote LIST recv failed", err);
            draw_status_now();
            ftp_recover_control_after_transfer_error("LIST failed: reconnect required");
            return 0;
        }
    }
}

static void load_local_path(void)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    g_local_count = 0;
    g_local_sel = -1;
    g_local_top = 0;
    g_last_local_click_index = -1;
    add_local_parent_entry();
    lock = Lock((CONST_STRPTR)g_local_path, ACCESS_READ);
    if (!lock) {
        g_local_count = 0;
        g_local_sel = -1;
        add_local_parent_entry();
        set_status("Cannot enter directory");
        draw_ui();
        return;
    }
    fib = (struct FileInfoBlock *)AllocMem(sizeof(*fib), MEMF_PUBLIC | MEMF_CLEAR);
    if (!fib) {
        UnLock(lock);
        set_status("No memory");
        draw_ui();
        return;
    }
    if (Examine(lock, fib)) {
        while (g_local_count < MAX_LOCAL_ENTRIES && ExNext(lock, fib)) {
            copy_limited(g_local_entries[g_local_count].name, NAME_BUF_SIZE, (const char *)fib->fib_FileName);
            g_local_entries[g_local_count].is_dir = fib->fib_DirEntryType > 0;
            ++g_local_count;
        }
    }
    FreeMem(fib, sizeof(*fib));
    UnLock(lock);
    sort_entries(g_local_entries, g_local_count);
    set_status("Local directory loaded");
    draw_ui();
}

static int ftp_connect_login(void)
{
    ULONG ip;
    UWORD control_port;
    int code;
    char start_remote_path[PATH_BUF_SIZE];

    copy_limited(start_remote_path, sizeof(start_remote_path), g_remote_path);

    if (g_transfer_busy) {
        set_status_draw("Busy");
        return 0;
    }
    if (!parse_port_field(g_port, &control_port)) {
        set_status_draw("Invalid port");
        return 0;
    }
    g_transfer_busy = 1;
    ftp_gui_disconnect_session(0);
    set_status("Connecting...");
    draw_ui();
    if (!g_sock_base) {
        g_sock_base = OpenLibrary((CONST_STRPTR)"bsdsocket.library", 1);
        if (!g_sock_base) {
            set_status_draw("Open bsdsocket failed");
            g_transfer_busy = 0;
            return 0;
        }
    }
    set_status_draw("Resolving host...");
    if (!resolve_host(g_sock_base, g_host, &ip)) {
        ftp_gui_disconnect_session("Connect failed: resolve failed");
        g_transfer_busy = 0;
        return 0;
    }
    set_status_draw("Connecting control socket...");
    g_ctrl_fd = open_connected_socket(g_sock_base, ip, control_port);
    if (g_ctrl_fd < 0) {
        if (status_is_timeout())
            ftp_gui_disconnect_session("Connect failed: timeout");
        else
            ftp_gui_disconnect_session("Connect failed: socket error");
        g_transfer_busy = 0;
        return 0;
    }
    set_status_draw("Waiting for greeting...");
    if (!read_response(g_sock_base, g_ctrl_fd, &code) || code >= 400) {
        if (status_is_timeout())
            ftp_gui_disconnect_session("Connect failed: timeout");
        else
            ftp_gui_disconnect_session("Connect failed: greeting");
        g_transfer_busy = 0;
        return 0;
    }
    set_status_draw("Sending USER...");
    if (!ftp_command(g_sock_base, g_ctrl_fd, "USER", g_user, &code)) {
        ftp_gui_disconnect_session(status_is_timeout() ? "Connect failed: timeout" : "Login failed");
        g_transfer_busy = 0;
        return 0;
    }
    set_status_draw("Sending PASS...");
    if (code == 331 && !ftp_command(g_sock_base, g_ctrl_fd, "PASS", g_pass, &code)) {
        ftp_gui_disconnect_session(status_is_timeout() ? "Connect failed: timeout" : "Login failed");
        g_transfer_busy = 0;
        return 0;
    }
    if (code != 230) {
        ftp_gui_disconnect_session("Login failed");
        g_transfer_busy = 0;
        return 0;
    }
    set_status_draw("Setting binary mode...");
    if (!ftp_command(g_sock_base, g_ctrl_fd, "TYPE", "I", &code) || code < 200 || code >= 300) {
        ftp_gui_disconnect_session(status_is_timeout() ? "Connect failed: timeout" : "Connect failed: TYPE I");
        g_transfer_busy = 0;
        return 0;
    }
    g_connected = 1;
    if (start_remote_path[0] && !text_equal(start_remote_path, "/")) {
        set_status_draw("Changing remote path...");
        if (!ftp_command(g_sock_base, g_ctrl_fd, "CWD", start_remote_path, &code) ||
            code < 200 || code >= 300) {
            ftp_gui_disconnect_session(status_is_timeout() ? "Connect failed: timeout" : "CWD failed");
            g_transfer_busy = 0;
            return 0;
        }
        copy_limited(g_remote_path, sizeof(g_remote_path), start_remote_path);
    }
    set_status_draw("Connected");
    if (!ftp_list_remote()) {
        g_transfer_busy = 0;
        return 0;
    }
    set_status_draw("Connected");
    g_transfer_busy = 0;
    return 1;
}

static int ftp_download_selected(void)
{
    int data_fd = -1;
    int code;
    int got;
    int err;
    BPTR file;
    LONG wrote;
    LONG total = 0;
    LONG next_progress = PROGRESS_STEP_BYTES;
    LONG expected_size = -1;
    char local_file[PATH_BUF_SIZE + NAME_BUF_SIZE];
    const char *remote;

    if (g_transfer_busy) {
        set_status_draw("Busy");
        return 0;
    }
    g_transfer_busy = 1;
    if (!g_connected) {
        set_status_draw("Not connected");
        g_transfer_busy = 0;
        return 0;
    }
    if (g_remote_sel < 0 || g_remote_sel >= g_remote_count) {
        set_status_draw("No remote file selected");
        g_transfer_busy = 0;
        return 0;
    }
    if (g_remote_entries[g_remote_sel].is_dir) {
        set_status_draw("Select a file, not a directory");
        g_transfer_busy = 0;
        return 0;
    }
    remote = g_remote_entries[g_remote_sel].name;
    if (!ftp_get_remote_size(remote, &expected_size)) {
        ftp_gui_disconnect_session("Download failed: reconnect required");
        g_transfer_busy = 0;
        return 0;
    }
    build_local_full_path(local_file, sizeof(local_file), basename_any(remote));
    file = Open((CONST_STRPTR)local_file, MODE_NEWFILE);
    if (!file) {
        set_status_draw("local file open failed");
        g_transfer_busy = 0;
        return 0;
    }
    data_fd = ftp_pasv_open_data("Download failed");
    if (data_fd < 0) {
        Close(file);
        ftp_gui_disconnect_session("Download failed: reconnect required");
        g_transfer_busy = 0;
        return 0;
    }
    if (!ftp_command(g_sock_base, g_ctrl_fd, "RETR", remote, &code) || (code != 125 && code != 150)) {
        close_data_socket(&data_fd);
        Close(file);
        set_status_draw("RETR failed");
        g_transfer_busy = 0;
        return 0;
    }
    set_status_draw("Downloading...");
    for (;;) {
        if (!wait_for_socket(g_sock_base, data_fd, 0)) {
            close_data_socket(&data_fd);
            Close(file);
            ftp_gui_disconnect_session("Download failed: reconnect required");
            g_transfer_busy = 0;
            return 0;
        }
        for (;;) {
            got = call_recv(g_sock_base, data_fd, g_data_buf, sizeof(g_data_buf), 0);
            if (got > 0) {
                wrote = Write(file, g_data_buf, got);
                if (wrote != got) {
                    close_data_socket(&data_fd);
                    Close(file);
                    ftp_recover_control_after_transfer_error("Download failed: reconnect required");
                    g_transfer_busy = 0;
                    return 0;
                }
                total += got;
                if (total >= next_progress) {
                    set_status_kb("Downloading", total);
                    draw_status_now();
                    while (next_progress <= total)
                        next_progress += PROGRESS_STEP_BYTES;
                }
                continue;
            }
            if (got == 0) {
                close_data_socket(&data_fd);
                Close(file);
                if (!ftp_read_final_transfer_reply("Download failed")) {
                    ftp_gui_disconnect_session("Download failed: reconnect required");
                    g_transfer_busy = 0;
                    return 0;
                }
                if (expected_size >= 0 && total < expected_size) {
                    ftp_gui_disconnect_session("Download incomplete: reconnect required");
                    g_transfer_busy = 0;
                    return 0;
                }
                set_status_kb("Download complete", total);
                load_local_path();
                set_status_kb("Download complete", total);
                draw_ui();
                g_transfer_busy = 0;
                return 1;
            }
            err = call_errno(g_sock_base);
            if (socket_retry_error(err))
                break;
            close_data_socket(&data_fd);
            Close(file);
            ftp_gui_disconnect_session(0);
            set_status_errno("Download recv failed", err);
            if (g_win)
                draw_ui();
            g_transfer_busy = 0;
            return 0;
        }
    }
}

static int ftp_upload_selected(void)
{
    int data_fd = -1;
    int code;
    BPTR file;
    LONG got;
    LONG total = 0;
    LONG next_progress = PROGRESS_STEP_BYTES;
    char local_file[PATH_BUF_SIZE + NAME_BUF_SIZE];
    const char *remote;

    if (g_transfer_busy) {
        set_status_draw("Busy");
        return 0;
    }
    g_transfer_busy = 1;
    if (!g_connected) {
        set_status_draw("Not connected");
        g_transfer_busy = 0;
        return 0;
    }
    if (g_local_sel < 0 || g_local_sel >= g_local_count) {
        set_status_draw("No local file selected");
        g_transfer_busy = 0;
        return 0;
    }
    if (text_equal(g_local_entries[g_local_sel].name, "..")) {
        set_status_draw("Select a file to upload");
        g_transfer_busy = 0;
        return 0;
    }
    if (g_local_entries[g_local_sel].is_dir) {
        set_status_draw("Cannot upload directory");
        g_transfer_busy = 0;
        return 0;
    }
    remote = g_local_entries[g_local_sel].name;
    build_local_full_path(local_file, sizeof(local_file), remote);
    file = Open((CONST_STRPTR)local_file, MODE_OLDFILE);
    if (!file) {
        set_status_draw("local file open failed");
        g_transfer_busy = 0;
        return 0;
    }
    if (!ftp_command(g_sock_base, g_ctrl_fd, "TYPE", "I", &code) || code < 200 || code >= 300) {
        Close(file);
        ftp_gui_disconnect_session("Upload failed: TYPE I");
        g_transfer_busy = 0;
        return 0;
    }
    data_fd = ftp_pasv_open_data("Upload failed");
    if (data_fd < 0) {
        Close(file);
        ftp_gui_disconnect_session("Upload failed: reconnect required");
        g_transfer_busy = 0;
        return 0;
    }
    if (!ftp_command(g_sock_base, g_ctrl_fd, "STOR", remote, &code) || (code != 125 && code != 150)) {
        close_data_socket(&data_fd);
        Close(file);
        set_status_draw("STOR failed");
        g_transfer_busy = 0;
        return 0;
    }
    set_status_draw("Uploading...");
    for (;;) {
        got = Read(file, g_data_buf, UPLOAD_CHUNK_SIZE);
        if (got > 0) {
            if (!upload_send_chunk(g_sock_base, data_fd, (const char *)g_data_buf, (int)got, &total)) {
                close_data_socket(&data_fd);
                Close(file);
                ftp_gui_disconnect_session("Upload failed: reconnect required");
                g_transfer_busy = 0;
                return 0;
            }
            if (total >= next_progress) {
                set_status_kb("Uploading", total);
                draw_status_now();
                while (next_progress <= total)
                    next_progress += PROGRESS_STEP_BYTES;
            }
            continue;
        }
        if (got == 0)
            break;
        close_data_socket(&data_fd);
        Close(file);
        ftp_gui_disconnect_session("Upload failed: reconnect required");
        g_transfer_busy = 0;
        return 0;
    }
    Close(file);
    upload_drain_socket(g_sock_base, data_fd);
    close_data_socket(&data_fd);
    if (!ftp_read_final_transfer_reply("Upload failed")) {
        ftp_gui_disconnect_session("Upload failed: reconnect required");
        g_transfer_busy = 0;
        return 0;
    }
    {
        LONG remote_size = -1;
        if (ftp_get_remote_size(remote, &remote_size) && remote_size >= 0 && remote_size != total) {
            set_status_draw("Upload size mismatch");
            g_transfer_busy = 0;
            return 0;
        }
    }
    set_status_kb("Upload complete", total);
    draw_status_now();
    if (!ftp_list_remote()) {
        ftp_gui_disconnect_session("Upload complete; reconnect required");
        g_transfer_busy = 0;
        return 0;
    }
    set_status_kb("Upload complete", total);
    draw_status_now();
    g_transfer_busy = 0;
    return 1;
}

static int dialog_button_hit(WORD mx, WORD my, WORD x, WORD y, WORD w, WORD h)
{
    return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

static void dialog_draw_button(struct Window *win, WORD x, WORD y, WORD w, WORD h, const char *label)
{
    if (!win)
        return;
    SetAPen(win->RPort, 1);
    Move(win->RPort, x, y);
    Draw(win->RPort, (WORD)(x + w), y);
    Draw(win->RPort, (WORD)(x + w), (WORD)(y + h));
    Draw(win->RPort, x, (WORD)(y + h));
    Draw(win->RPort, x, y);
    Move(win->RPort, (WORD)(x + 8), (WORD)(y + 12));
    Text(win->RPort, (STRPTR)label, text_len(label));
}

static void dialog_text(struct Window *win, WORD x, WORD y, const char *text)
{
    if (!win || !text)
        return;
    SetAPen(win->RPort, 1);
    Move(win->RPort, x, y);
    Text(win->RPort, (STRPTR)text, text_len(text));
}

static int confirm_delete_dialog(const char *where, const char *name)
{
    struct NewWindow nw;
    struct Window *win;
    struct IntuiMessage *msg;
    ULONG cls;
    UWORD code;
    WORD mx;
    WORD my;
    int running = 1;
    int result = 0;

    nw.LeftEdge = 40;
    nw.TopEdge = 35;
    nw.Width = 420;
    nw.Height = 92;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_REFRESHWINDOW;
    nw.Flags = WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_ACTIVATE | WFLG_SMART_REFRESH;
    nw.FirstGadget = 0;
    nw.CheckMark = 0;
    nw.Title = (STRPTR)"Confirm delete";
    nw.Screen = 0;
    nw.BitMap = 0;
    nw.MinWidth = 240;
    nw.MinHeight = 70;
    nw.MaxWidth = 640;
    nw.MaxHeight = 200;
    nw.Type = WBENCHSCREEN;

    win = OpenWindow(&nw);
    if (!win) {
        set_status_draw("Delete confirm failed");
        return 0;
    }

    while (running) {
        SetDrMd(win->RPort, JAM1);
        SetAPen(win->RPort, 0);
        RectFill(win->RPort, 0, 10, (WORD)(win->Width - 1), (WORD)(win->Height - 1));
        SetAPen(win->RPort, 1);
        dialog_text(win, 12, 25, "Delete selected file?");
        dialog_text(win, 12, 40, where ? where : "");
        dialog_text(win, 12, 55, name ? name : "");
        dialog_draw_button(win, 250, 66, 58, 16, "OK");
        dialog_draw_button(win, 320, 66, 70, 16, "Cancel");

        Wait(1UL << win->UserPort->mp_SigBit);
        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) {
            cls = msg->Class;
            code = msg->Code;
            mx = msg->MouseX;
            my = msg->MouseY;
            ReplyMsg((struct Message *)msg);
            if (cls == IDCMP_CLOSEWINDOW) {
                running = 0;
            } else if (cls == IDCMP_REFRESHWINDOW) {
                BeginRefresh(win);
                EndRefresh(win, TRUE);
                break;
            } else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTUP) {
                if (dialog_button_hit(mx, my, 250, 66, 58, 16)) {
                    result = 1;
                    running = 0;
                } else if (dialog_button_hit(mx, my, 320, 66, 70, 16)) {
                    running = 0;
                }
            }
        }
    }
    CloseWindow(win);
    return result;
}

static void show_info_dialog(void)
{
    struct NewWindow nw;
    struct Window *win;
    struct IntuiMessage *msg;
    ULONG cls;
    UWORD code;
    WORD mx;
    WORD my;
    int running = 1;

    nw.LeftEdge = 10;
    nw.TopEdge = 20;
    nw.Width = 620;
    nw.Height = 110;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_REFRESHWINDOW;
    nw.Flags = WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_ACTIVATE | WFLG_SMART_REFRESH;
    nw.FirstGadget = 0;
    nw.CheckMark = 0;
    nw.Title = (STRPTR)"MiniFTP Info";
    nw.Screen = 0;
    nw.BitMap = 0;
    nw.MinWidth = 300;
    nw.MinHeight = 80;
    nw.MaxWidth = 640;
    nw.MaxHeight = 200;
    nw.Type = WBENCHSCREEN;

    win = OpenWindow(&nw);
    if (!win) {
        set_status_draw("Info window failed");
        return;
    }

    while (running) {
        SetDrMd(win->RPort, JAM1);
        SetAPen(win->RPort, 0);
        RectFill(win->RPort, 0, 10, (WORD)(win->Width - 1), (WORD)(win->Height - 1));
        SetAPen(win->RPort, 1);
        dialog_text(win, 12, 25, "MiniFTP for Kick1.3");
        dialog_text(win, 12, 40, "Version: " MINI_FTP_VERSION);
        dialog_text(win, 12, 55, "by Marcel Jaehne");
        dialog_text(win, 12, 70, "(c) 2026");
        dialog_text(win, 12, 85, "If you want to buy me a coffe, send me a buck to: https://paypal.me/mytubefree");
        dialog_draw_button(win, 290, 92, 58, 16, "OK");

        Wait(1UL << win->UserPort->mp_SigBit);
        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) {
            cls = msg->Class;
            code = msg->Code;
            mx = msg->MouseX;
            my = msg->MouseY;
            ReplyMsg((struct Message *)msg);
            if (cls == IDCMP_CLOSEWINDOW) {
                running = 0;
            } else if (cls == IDCMP_REFRESHWINDOW) {
                BeginRefresh(win);
                EndRefresh(win, TRUE);
                break;
            } else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTUP) {
                if (dialog_button_hit(mx, my, 290, 92, 58, 16))
                    running = 0;
            }
        }
    }
    CloseWindow(win);
    if (g_win)
        draw_ui();
}

static int local_delete_selected(void)
{
    char local_file[PATH_BUF_SIZE + NAME_BUF_SIZE];
    const char *local;

    if (g_transfer_busy) {
        set_status_draw("Busy");
        return 0;
    }
    if (g_local_sel < 0 || g_local_sel >= g_local_count) {
        set_status_draw("No local file selected");
        return 0;
    }
    if (text_equal(g_local_entries[g_local_sel].name, "..")) {
        set_status_draw("Select a file to delete");
        return 0;
    }
    if (g_local_entries[g_local_sel].is_dir) {
        set_status_draw("Cannot delete directory");
        return 0;
    }

    local = g_local_entries[g_local_sel].name;
    if (!confirm_delete_dialog("Local file:", local)) {
        set_status_draw("Delete cancelled");
        return 0;
    }

    build_local_full_path(local_file, sizeof(local_file), local);
    if (!DeleteFile((CONST_STRPTR)local_file)) {
        set_status_draw("Local delete failed");
        return 0;
    }
    load_local_path();
    set_status_draw("Delete complete");
    return 1;
}

static int ftp_delete_selected(void)
{
    int code;
    const char *remote;

    if (g_transfer_busy) {
        set_status_draw("Busy");
        return 0;
    }
    g_transfer_busy = 1;
    if (!g_connected) {
        set_status_draw("Not connected");
        g_transfer_busy = 0;
        return 0;
    }
    if (g_remote_sel < 0 || g_remote_sel >= g_remote_count) {
        set_status_draw("No remote file selected");
        g_transfer_busy = 0;
        return 0;
    }
    if (g_remote_entries[g_remote_sel].is_dir) {
        set_status_draw("Select a file, not a directory");
        g_transfer_busy = 0;
        return 0;
    }

    remote = g_remote_entries[g_remote_sel].name;
    if (!confirm_delete_dialog("FTP file:", remote)) {
        set_status_draw("Delete cancelled");
        g_transfer_busy = 0;
        return 0;
    }
    set_status_draw("Deleting...");

    if (!ftp_command(g_sock_base, g_ctrl_fd, "DELE", remote, &code)) {
        ftp_gui_disconnect_session("Delete failed: reconnect required");
        g_transfer_busy = 0;
        return 0;
    }
    if (code == 250 || code == 200 || code == 202) {
        set_status_draw("Delete complete");
        if (!ftp_list_remote()) {
            g_transfer_busy = 0;
            return 0;
        }
        if (g_remote_sel >= g_remote_count)
            g_remote_sel = g_remote_count - 1;
        g_remote_top = clamp_top(g_remote_top, g_remote_count);
        set_status_draw("Delete complete");
        g_transfer_busy = 0;
        return 1;
    }
    if (code == 450 || code == 550)
        set_status("Remote file not found or permission denied");
    else if (code == 530)
        set_status("Permission denied");
    else
        set_status("FTP delete failed");
    draw_status_now();
    g_transfer_busy = 0;
    return 0;
}

static int delete_selected(void)
{
    if (g_active_pane == ACTIVE_PANE_LOCAL)
        return local_delete_selected();
    return ftp_delete_selected();
}

static int ftp_remote_enter_selected(void)
{
    int code;
    const char *remote;

    if (!g_connected || g_remote_sel < 0 || g_remote_sel >= g_remote_count)
        return 0;
    if (!g_remote_entries[g_remote_sel].is_dir)
        return 0;
    if (g_transfer_busy) {
        set_status_draw("Busy");
        return 0;
    }
    g_transfer_busy = 1;

    remote = g_remote_entries[g_remote_sel].name;
    if (text_equal(remote, "..")) {
        ftp_debug_puts("FTP GUI CDUP start\n");
        set_status_draw("Changing directory...");
        if (!ftp_command(g_sock_base, g_ctrl_fd, "CDUP", 0, &code) ||
            code < 200 || code >= 300) {
            ftp_gui_disconnect_session("Directory failed: reconnect required");
            g_transfer_busy = 0;
            return 0;
        }
        ftp_debug_puts("FTP GUI CDUP ok\n");
        remote_path_parent();
    } else {
        ftp_debug_puts("FTP GUI CWD start ");
        ftp_debug_puts(remote);
        ftp_debug_puts("\n");
        set_status_draw("Changing directory...");
        if (!ftp_command(g_sock_base, g_ctrl_fd, "CWD", remote, &code) ||
            code < 200 || code >= 300) {
            ftp_gui_disconnect_session("Directory failed: reconnect required");
            g_transfer_busy = 0;
            return 0;
        }
        ftp_debug_puts("FTP GUI CWD ok\n");
        remote_path_enter(remote);
    }

    if (!ftp_list_remote()) {
        g_transfer_busy = 0;
        return 0;
    }
    set_status_draw("Directory loaded");
    g_transfer_busy = 0;
    return 1;
}

static int local_click_is_double(int index)
{
    ULONG seconds;
    ULONG micros;
    int is_double = 0;

    CurrentTime(&seconds, &micros);
    if (index == g_last_local_click_index &&
        DoubleClick(g_last_local_click_seconds,
                    g_last_local_click_micros,
                    seconds,
                    micros))
        is_double = 1;

    g_last_local_click_index = index;
    g_last_local_click_seconds = seconds;
    g_last_local_click_micros = micros;
    return is_double;
}

static int remote_click_is_double(int index)
{
    ULONG seconds;
    ULONG micros;
    int is_double = 0;

    CurrentTime(&seconds, &micros);
    if (index == g_last_remote_click_index &&
        DoubleClick(g_last_remote_click_seconds,
                    g_last_remote_click_micros,
                    seconds,
                    micros))
        is_double = 1;

    g_last_remote_click_index = index;
    g_last_remote_click_seconds = seconds;
    g_last_remote_click_micros = micros;
    return is_double;
}

static int local_enter_selected(void)
{
    const char *local;

    if (g_local_sel < 0 || g_local_sel >= g_local_count)
        return 0;
    if (!g_local_entries[g_local_sel].is_dir)
        return 0;

    local = g_local_entries[g_local_sel].name;
    if (text_equal(local, "..")) {
        if (!local_path_parent()) {
            set_status_draw("Already at volume root");
            return 0;
        }
    } else {
        local_path_enter(local);
    }

    load_local_path();
    return 1;
}

static void handle_mouse_down(WORD mx, WORD my)
{
    g_pressed_button = button_at(mx, my);
    if (g_pressed_button != BUTTON_NONE)
        draw_button_by_id(g_pressed_button, 1);

    if (in_rect(mx, my, PASS_FIELD_X, PASS_FIELD_Y, PASS_FIELD_W, PASS_FIELD_H)) {
        g_pass_active = 1;
        draw_password_field();
    } else {
        if (g_pass_active) {
            g_pass_active = 0;
            draw_password_field();
        }
    }

    if (list_scrollbar_hit(mx, my, LOCAL_X, LOCAL_Y, LOCAL_W, LOCAL_H)) {
        g_scroll_drag = SCROLL_LOCAL;
        update_scroll_from_mouse(g_scroll_drag, my);
    } else if (list_scrollbar_hit(mx, my, REMOTE_X, REMOTE_Y, REMOTE_W, REMOTE_H)) {
        g_scroll_drag = SCROLL_REMOTE;
        update_scroll_from_mouse(g_scroll_drag, my);
    } else {
        g_scroll_drag = SCROLL_NONE;
    }
}

static void handle_mouse_move(WORD mx, WORD my)
{
    (void)mx;
    if (g_scroll_drag != SCROLL_NONE)
        update_scroll_from_mouse(g_scroll_drag, my);
}

static void handle_mouse_up(WORD mx, WORD my)
{
    int released_button = button_at(mx, my);
    int pressed_button = g_pressed_button;

    if (pressed_button != BUTTON_NONE) {
        draw_button_by_id(pressed_button, 0);
        g_pressed_button = BUTTON_NONE;
    }

    if (pressed_button == BUTTON_CONNECT && released_button == pressed_button) {
        ftp_connect_login();
    } else if (pressed_button == BUTTON_LOAD && released_button == pressed_button) {
        load_local_path();
    } else if (pressed_button == BUTTON_UPLOAD && released_button == pressed_button) {
        ftp_upload_selected();
    } else if (pressed_button == BUTTON_DOWNLOAD && released_button == pressed_button) {
        ftp_download_selected();
    } else if (pressed_button == BUTTON_DELETE && released_button == pressed_button) {
        delete_selected();
    } else if (pressed_button == BUTTON_INFO && released_button == pressed_button) {
        show_info_dialog();
    } else if (pressed_button == BUTTON_NONE && in_rect(mx, my, LOCAL_X, LOCAL_Y, LOCAL_W, LOCAL_H)) {
        int row = (my - LOCAL_Y - 2) / ROW_H;
        int index = g_local_top + row;
        if (!list_scrollbar_hit(mx, my, LOCAL_X, LOCAL_Y, LOCAL_W, LOCAL_H) &&
            row >= 0 && row < g_visible_rows && index >= 0 && index < g_local_count) {
            int is_double = local_click_is_double(index);
            g_active_pane = ACTIVE_PANE_LOCAL;
            g_local_sel = index;
            draw_ui();
            if (is_double)
                local_enter_selected();
        }
    } else if (pressed_button == BUTTON_NONE && in_rect(mx, my, REMOTE_X, REMOTE_Y, REMOTE_W, REMOTE_H)) {
        int row = (my - REMOTE_Y - 2) / ROW_H;
        int index = g_remote_top + row;
        if (!list_scrollbar_hit(mx, my, REMOTE_X, REMOTE_Y, REMOTE_W, REMOTE_H) &&
            row >= 0 && row < g_visible_rows && index >= 0 && index < g_remote_count) {
            int is_double = remote_click_is_double(index);
            g_active_pane = ACTIVE_PANE_REMOTE;
            g_remote_sel = index;
            draw_ui();
            if (is_double)
                ftp_remote_enter_selected();
        }
    }
    g_scroll_drag = SCROLL_NONE;
}

static void handle_password_key(UWORD code)
{
    LONG len;

    if (!g_pass_active)
        return;

    len = text_len(g_pass);
    if (code == 8 || code == 127) {
        if (len > 0)
            g_pass[len - 1] = 0;
    } else if (code == 13 || code == 10) {
        g_pass_active = 0;
    } else if (code >= 32 && code < 127) {
        if (len < PASS_BUF_SIZE - 1) {
            g_pass[len] = (char)code;
            g_pass[len + 1] = 0;
        }
    }
    draw_password_field();
}

static void close_network(void)
{
    ftp_close_data();
    if (g_sock_base && g_ctrl_fd >= 0) {
        int ignored;
        ftp_command(g_sock_base, g_ctrl_fd, "QUIT", 0, &ignored);
        ftp_close_control();
    }
    clear_remote_session_state();
    if (g_sock_base) {
        CloseLibrary(g_sock_base);
        g_sock_base = 0;
    }
}

static void init_string_info(struct StringInfo *si)
{
    WORD len;

    if (!si || !si->Buffer)
        return;

    len = (WORD)text_len((const char *)si->Buffer);
    si->BufferPos = len;
    si->NumChars = len;
    si->DispPos = 0;
    if (si->UndoBuffer)
        copy_limited((char *)si->UndoBuffer, si->MaxChars, (const char *)si->Buffer);
}

static void init_string_gadgets(void)
{
    init_string_info(&g_host_si);
    init_string_info(&g_port_si);
    init_string_info(&g_user_si);
    init_string_info(&g_path_si);
}

int main(int argc, char **argv)
{
    int running = 1;
    int from_workbench = 0;
    int autoconnect = 0;
    struct WBStartup *wbmsg = 0;
    struct IntuiMessage *msg;
    ULONG cls;
    UWORD code;
    WORD mx;
    WORD my;

    if (argc == 0 && argv) {
        from_workbench = 1;
        wbmsg = (struct WBStartup *)argv;
        apply_workbench_tooltypes(wbmsg, &autoconnect);
    }

    ftp_debug_puts(MINI_FTP_GUI_TITLE "\n");
    ftp_debug_puts("GUI START phase=entry\n");
    ftp_debug_puts("GUI START phase=open-libs\n");

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR)"intuition.library", 33);
    if (!IntuitionBase) {
        gui_puts("intuition.library open failed\n");
        return 20;
    }
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 33);
    if (!GfxBase) {
        gui_puts("graphics.library open failed\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return 20;
    }

    init_string_gadgets();
    set_initial_window_size();
    update_layout();
    ftp_debug_puts("GUI START phase=open-window\n");
    gui_log_window_geometry();
    g_win = OpenWindow(&g_new_window);
    if (!g_win) {
        gui_puts("window open failed\n");
        gui_puts("visual/screen issue or not enough memory\n");
        CloseLibrary((struct Library *)GfxBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 20;
    }
    gui_log_opened_window();

    if (!attach_string_gadgets()) {
        gui_puts("gadget allocation failed\n");
        CloseWindow(g_win);
        CloseLibrary((struct Library *)GfxBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 20;
    }

    SetAPen(g_win->RPort, 1);
    SetDrMd(g_win->RPort, JAM1);
    load_local_path();
    if (from_workbench)
        set_status("Ready");
    else
        set_status("Not connected");
    draw_ui();

    ftp_debug_puts("GUI START phase=open-bsdsocket\n");
    g_sock_base = OpenLibrary((CONST_STRPTR)"bsdsocket.library", 1);
    if (!g_sock_base) {
        gui_puts("bsdsocket.library open failed\n");
        set_status("bsdsocket.library open failed");
        draw_ui();
        CloseWindow(g_win);
        CloseLibrary((struct Library *)GfxBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 20;
    }

    if (autoconnect) {
        if (g_host[0] && g_user[0])
            ftp_connect_login();
        else
            set_status_draw("Autoconnect needs HOST and USER");
    }

    ftp_debug_puts("GUI START phase=event-loop\n");
    while (running) {
        Wait(1UL << g_win->UserPort->mp_SigBit);
        while ((msg = (struct IntuiMessage *)GetMsg(g_win->UserPort))) {
            cls = msg->Class;
            code = msg->Code;
            mx = msg->MouseX;
            my = msg->MouseY;
            ReplyMsg((struct Message *)msg);
            if (cls == IDCMP_CLOSEWINDOW) {
                running = 0;
            } else if (cls == IDCMP_NEWSIZE) {
                g_pressed_button = BUTTON_NONE;
                update_layout();
                draw_ui();
            } else if (cls == IDCMP_REFRESHWINDOW) {
                g_pressed_button = BUTTON_NONE;
                BeginRefresh(g_win);
                draw_ui();
                EndRefresh(g_win, TRUE);
            } else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTDOWN) {
                handle_mouse_down(mx, my);
            } else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTUP) {
                handle_mouse_up(mx, my);
            } else if (cls == IDCMP_MOUSEMOVE) {
                handle_mouse_move(mx, my);
            } else if (cls == IDCMP_VANILLAKEY) {
                handle_password_key(code);
            } else if (cls == IDCMP_GADGETUP) {
                draw_ui();
            }
        }
    }

    close_network();
    CloseWindow(g_win);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
