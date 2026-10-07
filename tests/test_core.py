#!/usr/bin/env python3
"""Exercise actual list and wait helpers with host-side Amiga API substitutes."""
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'src/MiniFTP.c').read_text()

def function(name):
    import re
    match = re.search(r'^static [^\n]+\b' + name + r'\([^;]*?\)\n\{', source, re.M)
    assert match, name
    start = match.start()
    end = match.end()
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t ULONG;
typedef unsigned char UBYTE;
#define MEMF_PUBLIC 1
#define MEMF_CLEAR 2
#define NAME_BUF_SIZE 64
#define TIMEOUT_SECONDS 20
#define AMITCP13_EINTR 4
struct FtpEntry { char name[NAME_BUF_SIZE]; UBYTE is_dir, selected; };
static struct FtpEntry *g_local_entries, *g_remote_entries;
static int g_local_incomplete, g_remote_incomplete;
static int allocations, fail_allocations, cancel, pumps;
static size_t largest_allocation = (size_t)-1;
static void *AllocMem(size_t size, int flags) {
    size_t *p;
    (void)flags;
    if (fail_allocations || size > largest_allocation) return NULL;
    p = calloc(1, size + sizeof(size_t));
    assert(p);
    *p = size;
    ++allocations;
    return p + 1;
}
static void FreeMem(void *ptr, size_t size) {
    size_t *p = (size_t *)ptr - 1;
    assert(*p == size);
    --allocations;
    free(p);
}
#define CopyMem(src, dst, n) memcpy(dst, src, n)
static int process_operation_events(void) { ++pumps; return !cancel; }
struct Library { int unused; };
static int g_read_fds, g_write_fds;
static ULONG g_wait_signals;
static struct { int tv_sec, tv_usec; } g_timeout;
#define AMITCP13_BSD_FD_ZERO(p) (*(p) = 0)
#define AMITCP13_BSD_FD_SET(fd,p) (*(p) = (fd)+1)
static int wait_calls, wait_result, wait_errno, cancel_at;
static int call_waitselect(struct Library *base, int nfds, void *r, void *w, void *t) {
    (void)base; (void)nfds; (void)t;
    assert((r != NULL) != (w != NULL));
    assert(g_timeout.tv_sec == 0 && g_timeout.tv_usec == 100000);
    ++wait_calls;
    if (wait_calls == cancel_at) cancel = 1;
    return wait_result;
}
static int call_errno(struct Library *base) { (void)base; return wait_errno; }
static char status[128];
static void set_status_draw(const char *s) { snprintf(status, sizeof(status), "%s", s); }
static void set_status_errno(const char *s, int err) { (void)err; set_status_draw(s); }
static void draw_status_now(void) {}
'''
helpers = '\n'.join(function(n) for n in [
    'free_entries', 'reserve_entries', 'upper_ascii', 'text_equal',
    'text_compare_ci', 'ftp_entry_before', 'sort_entries', 'entry_is_real',
    'snapshot_entries', 'wait_for_socket'])
main = r'''
int main(void) {
    struct FtpEntry *copy = NULL, *old;
    int i, count;
    for (i = 0; i < 5001; ++i) {
        assert(reserve_entries(&g_local_entries, i + 1));
        snprintf(g_local_entries[i].name, NAME_BUF_SIZE, "%05d", 5000 - i);
        g_local_entries[i].selected = (i % 2 == 0);
        g_local_entries[i].is_dir = (i % 3 == 0);
    }
    strcpy(g_local_entries[0].name, "..");
    sort_entries(g_local_entries, 5001);
    assert(strcmp(g_local_entries[0].name, "..") == 0);
    for (i = 2; i < 5001; ++i)
        assert(!ftp_entry_before(&g_local_entries[i], &g_local_entries[i - 1]));
    copy = snapshot_entries(g_local_entries, 5001, 0, &count);
    assert(copy && count == 5000);
    /* Callers may reduce their logical count; freeing still uses allocated size. */
    free_entries(copy);
    copy = snapshot_entries(g_local_entries, 5001, 1, &count);
    assert(copy && count == 2500);
    free_entries(copy);
    old = g_local_entries;
    fail_allocations = 1;
    assert(!reserve_entries(&g_local_entries, 20000));
    assert(old == g_local_entries && strcmp(old[0].name, "..") == 0);
    assert(!snapshot_entries(g_local_entries, 5001, 0, &count));
    fail_allocations = 0;
    g_local_incomplete = 1;
    assert(!snapshot_entries(g_local_entries, 5001, 0, &count));
    g_local_incomplete = 0;
    cancel = 1;
    assert(!snapshot_entries(g_local_entries, 5001, 0, &count));
    cancel = 0;
    free_entries(g_local_entries);
    g_local_entries = NULL;
    /* If a doubled allocation fails, a smaller exact allocation can succeed. */
    largest_allocation = sizeof(ULONG) + 33 * sizeof(struct FtpEntry);
    assert(reserve_entries(&g_local_entries, 32));
    assert(reserve_entries(&g_local_entries, 33));
    free_entries(g_local_entries);
    g_local_entries = NULL;
    assert(allocations == 0);
    assert(!reserve_entries(&g_local_entries, -1));
    assert(!reserve_entries(&g_local_entries, 0x7fffffff));
    free_entries(NULL);
    wait_result = 0; cancel_at = 3;
    assert(!wait_for_socket(NULL, 2, 0));
    assert(wait_calls == 3 && cancel);
    cancel = 0; cancel_at = 0; wait_calls = 0;
    assert(!wait_for_socket(NULL, 2, 1));
    assert(wait_calls == TIMEOUT_SECONDS * 10);
    assert(strcmp(status, "Timeout waiting for server") == 0);
    wait_calls = 0; wait_result = 1;
    assert(wait_for_socket(NULL, 2, 1) && wait_calls == 1);
    wait_result = -1; wait_errno = 9;
    assert(!wait_for_socket(NULL, 2, 0));
    wait_errno = AMITCP13_EINTR; cancel_at = wait_calls + 2;
    assert(!wait_for_socket(NULL, 2, 0) && cancel);
    assert(allocations == 0);
    puts("PASS: large lists, sorting, snapshots, allocation failures, cancellation and socket waits");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='miniftp-test-') as tmp:
    c = Path(tmp) / 'core.c'
    binary = Path(tmp) / 'core'
    c.write_text(prefix + helpers + main)
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-g', str(c), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
