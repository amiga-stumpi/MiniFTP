#!/usr/bin/env python3
"""Run the production transfer functions against deterministic file/socket fakes."""
from pathlib import Path
import re
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'src/MiniFTP.c').read_text()
def function(name):
    match = re.search(r'^static [^\n]+\b' + name + r'\([^;]*?\)\n\{', source, re.M)
    assert match, name
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]

prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint32_t ULONG;
typedef int32_t LONG;
typedef unsigned char UBYTE;
typedef int BPTR;
typedef void *APTR;
typedef const char *CONST_STRPTR;
#define MEMF_PUBLIC 1
#define MODE_NEWFILE 1
#define MODE_OLDFILE 2
#define PATH_BUF_SIZE 512
#define NAME_BUF_SIZE 256
#define UPLOAD_RETRY_LIMIT 64
#define AMITCP13_IPC_PAYLOAD_MAX 2048
struct Library { int unused; };
static struct Library *g_sock_base;
static int g_connected, g_ctrl_fd, g_transfer_busy;
static UBYTE g_data_buf[2048];
static int cancel, pumps, cancel_after;
static ULONG clock_ms;
static char g_status[128];
static int draws;
static void CurrentTime(ULONG *s, ULONG *us) { *s=clock_ms/1000; *us=(clock_ms%1000)*1000; }
static int process_operation_events(void) { ++pumps; ++clock_ms; if (cancel_after && pumps>=cancel_after) cancel=1; return !cancel; }
static void set_status(const char *s) { snprintf(g_status,sizeof(g_status),"%s",cancel ? "Cancelled" : s); }
static void set_status_draw(const char *s) { set_status(s); ++draws; }
static void set_status_kb(const char *s, LONG n) { (void)n; set_status(s); }
static void draw_status_now(void) { ++draws; }
static void append_status_dec(LONG n) { (void)n; }
static void append_text(char *dst,int size,const char *s) { strncat(dst,s,size-strlen(dst)-1); }
static void set_status_errno(const char *s,int err) { (void)err; set_status(s); }
static void ftp_debug_puts(const char *s) { (void)s; }
static void ftp_debug_dec(LONG n) { (void)n; }
static int allocations, alloc_ceiling;
static void *AllocMem(ULONG n,int flags) {
    ULONG *p; (void)flags;
    if (n>(ULONG)alloc_ceiling) return NULL;
    p=calloc(1,n+sizeof(ULONG)); assert(p); *p=n; ++allocations; return p+1;
}
static void FreeMem(void *ptr,ULONG n) { ULONG *p=(ULONG*)ptr-1; assert(*p==n); --allocations; free(p); }
#define CopyMem(src,dst,n) memcpy(dst,src,n)
static UBYTE local[40000], remote[40000], output[40000];
static int local_size, remote_size, output_size, local_pos, remote_pos;
static int open_files, open_sockets, read_calls, write_calls, send_calls, recv_calls, waits;
static int write_limit, write_fail, close_result, read_fail, send_limit, send_stalls, net_error, wait_fail;
static int recv_limit, corrupt_verify, final_fail, size_supported, size_mismatch, retr_calls;
static int socket_errno;
static BPTR Open(CONST_STRPTR path,int mode) { (void)path; assert(!open_files); ++open_files; if(mode==MODE_NEWFILE)output_size=0; else local_pos=0; return 1; }
static LONG Close(BPTR file) { assert(file && open_files==1); --open_files; return close_result; }
static LONG Read(BPTR file,void *data,ULONG n) {
    assert(file && open_files); ++read_calls;
    if(read_fail)return -1;
    if(n>(ULONG)(local_size-local_pos))n=local_size-local_pos;
    memcpy(data,local+local_pos,n); local_pos+=n; return n;
}
static LONG Write(BPTR file,const void *data,ULONG n) {
    assert(file && open_files); ++write_calls;
    if(write_fail)return -1;
    if(write_limit && n>(ULONG)write_limit)n=write_limit;
    assert(output_size+(int)n<=40000); memcpy(output+output_size,data,n); output_size+=n; return n;
}
static int call_errno(struct Library *base) { (void)base; return socket_errno; }
static int socket_retry_error(int err) { return err==35; }
static int call_send(struct Library *base,int fd,const void *data,int n,int flags) {
    (void)base;(void)flags;assert(fd==2 && open_sockets);++send_calls;
    assert(n<=2048);
    if(net_error) {socket_errno=9;return -1;}
    if(send_stalls && send_calls%5==0) {socket_errno=35;return -1;}
    if(send_limit && n>send_limit)n=send_limit;
    memcpy(remote+remote_size,data,n);remote_size+=n;return n;
}
static int call_recv(struct Library *base,int fd,void *data,int n,int flags) {
    (void)base;(void)flags;assert(fd==2 && open_sockets);++recv_calls;
    if(net_error) {socket_errno=9;return -1;}
    if(recv_limit && n>recv_limit)n=recv_limit;
    if(n>remote_size-remote_pos)n=remote_size-remote_pos;
    memcpy(data,remote+remote_pos,n);
    if(corrupt_verify && n && remote_pos==0)((UBYTE*)data)[0]^=1;
    remote_pos+=n;return n;
}
static int wait_for_socket(struct Library *base,int fd,int write) { (void)base;(void)fd;(void)write;++waits;return !wait_fail && !cancel; }
static int ftp_command(struct Library *base,int fd,const char *cmd,const char *arg,int *code) {
    (void)base;(void)fd;(void)arg;
    if(!process_operation_events())return 0;
    *code=200;
    if(!strcmp(cmd,"STOR")){remote_size=0;*code=150;}
    if(!strcmp(cmd,"RETR")){remote_pos=0;++retr_calls;*code=150;}
    return 1;
}
static int ftp_pasv_open_data(const char *error,const char *progress) { (void)error;set_status(progress);assert(!open_sockets);open_sockets=1;return 2; }
static void close_data_socket(int *fd) { if(*fd>=0){assert(open_sockets==1);--open_sockets;}*fd=-1; }
static int ftp_get_remote_size(const char *name,LONG *size) { (void)name;*size=size_supported?remote_size+size_mismatch:-1;return 1; }
static int ftp_read_final_transfer_reply(const char *error) { if(final_fail){set_status(error);return 0;}return !cancel; }
static void ftp_gui_disconnect_session(const char *status) { assert(!open_sockets);g_connected=0;if(status)set_status(status); }
static void build_local_full_path(char *out,int size,const char *name) { snprintf(out,size,"RAM:%s",name); }
static void upload_drain_socket(struct Library *base,int fd) { (void)base;(void)fd; }
'''
helpers = source.split('/* One transfer at a time;',1)[1].split('static int in_rect(',1)[0]
helpers = '/* One transfer at a time;' + helpers
program = prefix + helpers + '\n'.join(function(n) for n in (
    'upload_send_chunk', 'ftp_download_file', 'ftp_upload_file'))
program += r'''
static void reset(void) {
    int i; assert(!allocations && !open_files && !open_sockets);
    cancel=pumps=cancel_after=0; clock_ms=0; draws=0;
    g_connected=1;g_transfer_busy=0;g_upload_chunk=512;g_size_unavailable=0;
    g_data_ms=g_size_check_ms=g_list_ms=0;
    local_size=remote_size=20003;output_size=local_pos=remote_pos=0;
    read_calls=write_calls=send_calls=recv_calls=waits=0;
    write_limit=write_fail=close_result=read_fail=send_limit=send_stalls=net_error=wait_fail=0;
    recv_limit=corrupt_verify=final_fail=size_mismatch=retr_calls=0;size_supported=1;
    alloc_ceiling=8192;g_status[0]=0;
    for(i=0;i<local_size;++i)local[i]=remote[i]=(UBYTE)(i*17+3);
}
static void cleaned(void) { assert(!allocations && !open_files && !open_sockets && !g_transfer_busy); }
int main(void) {
    int sizes[]={128,512,1024,2048}, i;
    struct TransferBuffer b;
    reset();transfer_buffer_init(&b);assert(b.capacity==8192);transfer_buffer_free(&b);
    alloc_ceiling=4096;transfer_buffer_init(&b);assert(b.capacity==4096);transfer_buffer_free(&b);
    alloc_ceiling=2048;transfer_buffer_init(&b);assert(b.capacity==2048);transfer_buffer_free(&b);
    alloc_ceiling=0;transfer_buffer_init(&b);assert(b.data==g_io_fallback);transfer_buffer_free(&b);cleaned();
    reset();recv_limit=37;assert(ftp_download_file("data.bin"));cleaned();
    assert(output_size==remote_size && !memcmp(output,remote,remote_size));
    assert(write_calls==3); /* 541 network blocks coalesced into three disk writes. */
    reset();write_limit=13;assert(ftp_download_file("data.bin"));cleaned();assert(!memcmp(output,remote,remote_size));
    reset();write_fail=1;assert(!ftp_download_file("data.bin"));cleaned();assert(!g_connected);
    /* OS 1.3 Close has no defined result: zero must not disconnect. */
    reset();close_result=0;assert(ftp_download_file("data.bin"));cleaned();
    assert(g_connected && !strcmp(g_status,"Download complete"));
    assert(output_size==remote_size && !memcmp(output,remote,remote_size));
    reset();close_result=1;assert(ftp_download_file("data.bin"));cleaned();
    assert(g_connected);
    reset();close_result=-123;assert(ftp_download_file("data.bin"));cleaned();
    assert(g_connected);
    reset();size_mismatch=1;assert(!ftp_download_file("data.bin"));cleaned();
    reset();cancel_after=8;assert(!ftp_download_file("data.bin"));cleaned();
    reset();net_error=1;assert(!ftp_download_file("data.bin"));cleaned();
    reset();final_fail=1;assert(!ftp_download_file("data.bin"));cleaned();
    reset();local_size=remote_size=0;assert(ftp_download_file("empty"));cleaned();assert(!write_calls);
    for(i=0;i<4;++i) {
        reset();g_upload_chunk=sizes[i];send_limit=71;send_stalls=1;
        assert(ftp_upload_file("data.bin"));cleaned();assert(retr_calls==0 && waits>0);
        assert(remote_size==local_size && !memcmp(remote,local,local_size));
        assert(read_calls==4); /* Three buffered reads plus EOF, no read-back. */
    }
    reset();assert(ftp_upload_file("data.bin"));cleaned();assert(waits==0);
    reset();alloc_ceiling=0;assert(ftp_upload_file("data.bin"));cleaned();
    reset();assert(ftp_upload_file("data.bin"));cleaned();assert(retr_calls==0);
    reset();size_supported=0;assert(ftp_upload_file("data.bin"));cleaned();
    assert(g_size_unavailable && strstr(g_status,"unverified"));
    reset();size_mismatch=1;assert(!ftp_upload_file("data.bin"));cleaned();
    reset();read_fail=1;assert(!ftp_upload_file("data.bin"));cleaned();
    reset();cancel_after=8;assert(!ftp_upload_file("data.bin"));cleaned();
    reset();send_stalls=1;wait_fail=1;assert(!ftp_upload_file("data.bin"));cleaned();
    reset();local_size=remote_size=0;assert(ftp_upload_file("empty"));cleaned();
    reset();g_progress_stamp=0;clock_ms=249;transfer_progress("test",1);assert(draws==0);
    clock_ms=250;transfer_progress("test",2);assert(draws==1);
    clock_ms=499;transfer_progress("test",3);assert(draws==1);
    clock_ms=500;transfer_progress("test",4);assert(draws==2);
    show_transfer_timings();
    puts("PASS: buffered upload/download, size checks without read-back, partial I/O, errors, cancellation, fallback and progress");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='miniftp-transfers-') as tmp:
    c=Path(tmp)/'transfers.c'; binary=Path(tmp)/'transfers'
    c.write_text(program)
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g',str(c),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
