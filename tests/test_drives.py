#!/usr/bin/env python3
"""Test the OS 1.3 mounted-volume snapshot without a running Amiga."""
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t ULONG;
typedef unsigned char UBYTE;
typedef uintptr_t BPTR;
#define BADDR(p) ((void*)(uintptr_t)(p))
#define DLT_DEVICE 0
#define DLT_DIRECTORY 1
#define DLT_VOLUME 2
#define MEMF_PUBLIC 1
#define MEMF_CLEAR 2
struct DeviceList { BPTR dl_Next; int dl_Type; void *dl_Task; BPTR dl_Name; };
struct DosInfo { BPTR di_DevInfo; };
struct RootNode { BPTR rn_Info; };
struct DosLibrary { struct RootNode *dl_Root; };
struct DriveEntry { char path[258]; char volume[256]; };
static struct DosInfo info;
static struct RootNode root;
static struct DosLibrary dos,*DOSBase=&dos;
static int forbidden,allocations,fail_alloc,change_once;
static struct DeviceList nodes[40];
static UBYTE names[40][256];
static void Forbid(void) { assert(!forbidden);forbidden=1; }
static void Permit(void) { assert(forbidden);forbidden=0; }
static void *AllocMem(ULONG n,int flags) {
    ULONG *p; (void)flags;assert(!forbidden);
    if(fail_alloc)return NULL;
    p=calloc(1,n+sizeof(ULONG));assert(p);*p=n;++allocations;
    if(change_once){info.di_DevInfo=(BPTR)&nodes[0];change_once=0;}
    return p+1;
}
static void FreeMem(void *ptr,ULONG n) { ULONG *p=(ULONG*)ptr-1;assert(!forbidden && *p==n);--allocations;free(p); }
static int text_compare_ci(const char *a,const char *b) { return strcmp(a,b); }
'''
program+='\n'.join(function(n) for n in ('copy_mounted_drives','snapshot_mounted_drives'))
program+=r'''
static void node(int index,int type,int task,const char *name,int next) {
    int n=strlen(name);assert(n<=255);
    names[index][0]=n;memcpy(names[index]+1,name,n);
    nodes[index].dl_Name=(BPTR)names[index];nodes[index].dl_Type=type;
    nodes[index].dl_Task=(void*)(uintptr_t)task;
    nodes[index].dl_Next=next>=0?(BPTR)&nodes[next]:0;
}
int main(void) {
    struct DriveEntry *entries;
    int count,i;ULONG bytes;
    root.rn_Info=(BPTR)&info;dos.dl_Root=&root;
    node(0,DLT_DEVICE,1,"DH0",1);node(1,DLT_VOLUME,1,"Workbench",2);
    node(2,DLT_DEVICE,2,"RAM",3);node(3,DLT_VOLUME,2,"Ram Disk",4);
    node(4,DLT_DIRECTORY,1,"SYS",5);node(5,DLT_DEVICE,3,"CON",6);
    node(6,DLT_VOLUME,0,"Removed disk",7);node(7,DLT_VOLUME,4,"Network",-1);
    info.di_DevInfo=(BPTR)nodes;
    entries=snapshot_mounted_drives(&count,&bytes);assert(entries && count==3 && !forbidden);
    assert(!strcmp(entries[0].path,"DH0:") && !strcmp(entries[0].volume,"Workbench"));
    assert(!strcmp(entries[1].path,"Network:"));assert(!strcmp(entries[2].path,"RAM:"));
    /* The dialog owns copies: later DOS node mutations cannot alter selection. */
    names[0][1]='X';assert(!strcmp(entries[0].path,"DH0:"));FreeMem(entries,bytes);
    info.di_DevInfo=0;entries=snapshot_mounted_drives(&count,&bytes);assert(entries && count==0);FreeMem(entries,bytes);
    fail_alloc=1;assert(!snapshot_mounted_drives(&count,&bytes));assert(!forbidden && bytes==0);fail_alloc=0;
    for(i=0;i<30;++i){char name[20];snprintf(name,sizeof(name),"Disk%02d",i);node(i,DLT_VOLUME,i+1,name,i<29?i+1:-1);}
    info.di_DevInfo=0;change_once=1;
    entries=snapshot_mounted_drives(&count,&bytes);assert(entries && count==30);
    assert(!strcmp(entries[29].path,"Disk29:"));FreeMem(entries,bytes);
    memset(names[0]+1,'x',255);names[0][0]=255;nodes[0].dl_Next=0;info.di_DevInfo=(BPTR)nodes;
    entries=snapshot_mounted_drives(&count,&bytes);assert(entries && count==1);
    assert(strlen(entries[0].path)==256 && entries[0].path[255]==':');FreeMem(entries,bytes);
    assert(!allocations && !forbidden);
    puts("PASS: mounted drives, handler filtering, BCPL names, list growth, copies, empty list and allocation failure");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='miniftp-drives-') as tmp:
    c=Path(tmp)/'drives.c';binary=Path(tmp)/'drives';c.write_text(program)
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(c),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
