#!/usr/bin/env python3
"""Exercise recursive transfer snapshots and directory refresh boundaries."""
from pathlib import Path
import re
import subprocess
import tempfile
source=(Path(__file__).resolve().parents[1]/'src/MiniFTP.c').read_text()
def function(name):
    m=re.search(r'^static [^\n]+\b'+name+r'\([^;]*?\)\n\{',source,re.M)
    assert m,name
    end,depth=m.end(),1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[m.start():end]
program=r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef unsigned char UBYTE;
struct FtpEntry { char name[256]; UBYTE is_dir,selected; };
static struct FtpEntry locals[4],remotes[4];
static struct FtpEntry *g_local_entries=locals,*g_remote_entries=remotes;
static int g_local_count,g_remote_count,g_connected,g_cancel_requested,g_close_requested,g_local_incomplete;
static int g_measuring_transfer;
static int g_size_unavailable,g_data_ms,g_size_check_ms,g_list_ms;
static char g_local_path[512],g_remote_path[512],g_status[128];
static int local_lists,remote_lists,files,live_snapshots,fail_file,fail_snapshot;
static void copy_limited(char *out,int n,const char *in) { snprintf(out,n,"%s",in); }
static int text_equal(const char *a,const char *b) { return !strcmp(a,b); }
static void set_status_draw(const char *s) { copy_limited(g_status,sizeof(g_status),g_cancel_requested?"Cancelled":s); }
static int process_operation_events(void) { return !g_cancel_requested; }
static int path_can_enter(const char *base,const char *name) { return strlen(base)+strlen(name)+2<512; }
static void enter(char *path,const char *name) { strcat(path,"/");strcat(path,name); }
static void parent(char *path) { char *p=strrchr(path,'/');assert(p);*p=0; }
static void local_path_enter(const char *name) { enter(g_local_path,name); }
static int local_path_parent(void) { parent(g_local_path);return 1; }
static int ftp_cwd_name(const char *name) { enter(g_remote_path,name);return 1; }
static int ftp_cdup_dir(void) { parent(g_remote_path);return 1; }
static int local_create_dir_in_current(const char *name) { (void)name;return 1; }
static int ftp_mkdir_if_needed(const char *name) { (void)name;return 1; }
static void list(const char *path,struct FtpEntry *entries,int *count) {
    const char *leaf=strrchr(path,'/');leaf=leaf?leaf+1:path;
    memset(entries,0,4*sizeof(*entries));
    if(!strcmp(leaf,"root")) {
        strcpy(entries[0].name,"a.bin");strcpy(entries[1].name,"sub");entries[1].is_dir=1;strcpy(entries[2].name,"b.bin");*count=3;
    } else if(!strcmp(leaf,"sub")) {
        strcpy(entries[0].name,"c.bin");strcpy(entries[1].name,"deep");entries[1].is_dir=1;strcpy(entries[2].name,"d.bin");*count=3;
    } else if(!strcmp(leaf,"deep")) {strcpy(entries[0].name,"e.bin");*count=1;}
    else *count=0;
}
static void load_local_path(void) { ++local_lists;list(g_local_path,g_local_entries,&g_local_count);set_status_draw("Local directory loaded"); }
static int ftp_list_remote(void) { ++remote_lists;list(g_remote_path,g_remote_entries,&g_remote_count);set_status_draw("Remote directory loaded");return 1; }
static struct FtpEntry *snapshot_entries(struct FtpEntry *e,int n,int selected,int *out) {
    struct FtpEntry *p;(void)selected;
    if(fail_snapshot)return NULL;
    p=malloc((n?n:1)*sizeof(*p));assert(p);memcpy(p,e,n*sizeof(*p));*out=n;++live_snapshots;return p;
}
static void free_entries(struct FtpEntry *p) { if(p){--live_snapshots;free(p);} }
static int file_transfer(const char *name) {
    static const char *expected[]={"a.bin","c.bin","e.bin","d.bin","b.bin"};
    assert(files<5 && !strcmp(name,expected[files]));++files;
    if(fail_file && files==fail_file){set_status_draw("Transfer failed");g_connected=0;return 0;}
    return 1;
}
static int ftp_download_file(const char *name) { return file_transfer(name); }
static int ftp_upload_file(const char *name) { return file_transfer(name); }
static void ftp_gui_disconnect_session(const char *s) { g_connected=0;g_remote_path[0]=0;if(s)set_status_draw(s); }
'''
program+='\n'.join(function(n) for n in ('ftp_download_remote_entry_recursive','ftp_upload_local_entry_recursive','begin_transfer_batch','finish_transfer_batch'))
program+=r'''
static void reset(void) {
    assert(!live_snapshots);strcpy(g_local_path,"RAM:");strcpy(g_remote_path,"base");
    g_connected=1;g_cancel_requested=g_close_requested=g_local_incomplete=0;
    local_lists=remote_lists=files=fail_file=fail_snapshot=0;begin_transfer_batch();
}
int main(void) {
    int ok;
    reset();assert(ftp_upload_local_entry_recursive("root",1));
    assert(files==5 && local_lists==3 && remote_lists==0 && !live_snapshots);
    assert(!strcmp(g_local_path,"RAM:") && !strcmp(g_remote_path,"base"));
    assert(finish_transfer_batch(1,"RAM:","base",1));assert(local_lists==4 && remote_lists==1 && !g_measuring_transfer);
    reset();assert(ftp_download_remote_entry_recursive("root",1));
    assert(files==5 && local_lists==0 && remote_lists==3 && !live_snapshots);
    assert(finish_transfer_batch(1,"RAM:","base",0));assert(local_lists==1 && remote_lists==4);
    reset();fail_file=3;ok=ftp_upload_local_entry_recursive("root",1);assert(!ok && !live_snapshots);
    assert(!strcmp(g_local_path,"RAM:"));assert(!finish_transfer_batch(ok,"RAM:","base",1));
    assert(!strcmp(g_status,"Transfer failed"));
    reset();fail_snapshot=1;assert(!ftp_download_remote_entry_recursive("root",1));
    assert(!strcmp(g_local_path,"RAM:") && !strcmp(g_remote_path,"base") && !live_snapshots);
    reset();g_cancel_requested=1;assert(!ftp_download_remote_entry_recursive("root",1));
    assert(!finish_transfer_batch(0,"RAM:","base",0));assert(!g_connected && local_lists==1 && remote_lists==0);
    assert(!strcmp(g_status,"Cancelled"));
    reset();g_size_unavailable=1;assert(finish_transfer_batch(1,"RAM:","base",1));
    assert(strstr(g_status,"unverified"));
    puts("PASS: nested snapshots, one final refresh, navigation recovery, failure/cancellation and unverified status");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='miniftp-batches-') as tmp:
    c=Path(tmp)/'batches.c';binary=Path(tmp)/'batches';c.write_text(program)
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(c),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
