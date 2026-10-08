#!/usr/bin/env python3
"""Check the production layout against different Intuition border widths."""
from pathlib import Path
import re
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'src/MiniFTP.c').read_text()
layout = source.split('static void update_layout(void)\n', 1)[1].split('\nstatic LONG text_len', 1)[0]
variables = re.findall(r'^static WORD ([A-Z_]+) =', source, re.M)
gadgets = sorted(set(re.findall(r'\b(g_\w+_gad)\.', layout)))
text = source.split('static void draw_text_bounded(', 1)[1].split('\nstatic void draw_box', 1)[0]
program = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef short WORD;
typedef long LONG;
typedef char *STRPTR;
#define CONTROL_H 84
#define STATUS_H 20
#define MIN_LIST_ROWS 4
#define HSCROLL_H 12
#define ROW_H 9
struct Window { WORD Width, Height, BorderRight, BorderBottom; void *RPort; };
static struct Window window, *g_win = &window;
static struct { WORD Width, Height, MinWidth, MinHeight; } g_new_window = {640,200,560,186};
struct Gadget { WORD LeftEdge, TopEdge, Width, Height; };
static int g_remote_left;
static int g_visible_rows, g_local_top, g_local_count, g_remote_top, g_remote_count;
static int clamp_top(int top, int count) { (void)top; (void)count; return 0; }
static LONG text_len(const char *s) { return strlen(s); }
static LONG TextLength(void *rp, STRPTR s, LONG n) { (void)rp; (void)s; return n * 8; }
static WORD pen_x, pen_y;
static LONG text_end;
static void Move(void *rp, WORD x, WORD y) { (void)rp; pen_x=x; pen_y=y; }
static void Text(void *rp, STRPTR s, LONG n) { (void)rp; (void)s; text_end=pen_x+n*8; }
'''
program += '\n'.join('static WORD ' + name + ';' for name in variables)
program += '\n' + '\n'.join('static struct Gadget ' + name + ';' for name in gadgets)
program += '\nstatic void update_layout(void)\n' + layout
program += '\nstatic void draw_text_bounded(' + text
program += '\nstatic int horizontal_limit(' + source.split('static int horizontal_limit(', 1)[1].split('\nstatic void update_remote_horizontal', 1)[0]
program += '\nstatic const char *remote_visible_text(' + source.split('static const char *remote_visible_text(', 1)[1].split('\nstatic void draw_remote_horizontal', 1)[0]
program += r'''
int main(void) {
    int widths[] = {560, 640, 800, 1000};
    int borders[] = {4, 18, 24};
    int heights[] = {186, 200, 256};
    int w, b, h;
    char long_text[512];
    memset(long_text, 'W', sizeof(long_text)-1);
    long_text[sizeof(long_text)-1] = 0;
    for (w=0; w<4; ++w) for (b=0; b<3; ++b) for (h=0; h<3; ++h) {
        int frame_right;
        window.Width=widths[w]; window.Height=heights[h];
        window.BorderRight=borders[b]; window.BorderBottom=9;
        update_layout();
        frame_right=window.Width-window.BorderRight-2;
        assert(BTN_OPEN_X+BTN_OPEN_W <= frame_right-4);
        assert(REMOTE_X+REMOTE_W == BTN_OPEN_X+BTN_OPEN_W);
        assert(STATUS_X+STATUS_W == REMOTE_X+REMOTE_W);
        assert(g_open_gad.LeftEdge == BTN_OPEN_X && g_open_gad.Width == BTN_OPEN_W);
        assert(g_path_gad.LeftEdge+g_path_gad.Width+2 < BTN_OPEN_X);
        assert(LOCAL_X+LOCAL_W < REMOTE_X && REMOTE_W >= 120);
        assert(STATUS_Y+17 < window.Height-window.BorderBottom);
        assert(REMOTE_Y+REMOTE_H+2+HSCROLL_H < STATUS_Y);
        g_remote_left = 0;
        assert(remote_visible_text(long_text, 80) == long_text);
        assert(horizontal_limit(long_text, 80) == 501);
        g_remote_left = 501;
        assert(strcmp(remote_visible_text(long_text, 80), long_text+501) == 0);
        assert(strcmp(remote_visible_text("short", 80), "short") == 0);
        draw_text_bounded(STATUS_X+6, STATUS_TEXT_Y, long_text, STATUS_W-12);
        assert(text_end <= STATUS_X+STATUS_W-6);
        draw_text_bounded(REMOTE_X+4, REMOTE_Y+10, long_text, REMOTE_W-17);
        assert(text_end < REMOTE_X+REMOTE_W-11);
        draw_text_xy(285, 45, long_text);
        assert(text_end <= frame_right-4);
    }
    puts("PASS: layout and text bounds at 36 window/border combinations");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='miniftp-layout-') as tmp:
    c = Path(tmp) / 'layout.c'
    binary = Path(tmp) / 'layout'
    c.write_text(program)
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', str(c), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
