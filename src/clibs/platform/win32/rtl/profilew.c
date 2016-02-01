/*
    Software License Agreement (BSD License)
    
    Copyright (c) 1997-2008, David Lindauer, (LADSoft).
    All rights reserved.
    
    Redistribution and use of this software in source and binary forms, with or without modification, are
    permitted provided that the following conditions are met:
    
    * Redistributions of source code must retain the above
      copyright notice, this list of conditions and the
      following disclaimer.
    
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the
      following disclaimer in the documentation and/or other
      materials provided with the distribution.
    
    * Neither the name of LADSoft nor the names of its
      contributors may be used to endorse or promote products
      derived from this software without specific prior
      written permission of LADSoft.
    
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
    WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
    ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
    ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define ROTR(x,bits) (((x << (16 - bits)) | (x >> bits)) & 0xffff)

#define HASH_SIZE 4096

#pragma rundown show_profile 50
#pragma startup init_profile 50
extern char **_argv;

unsigned long long _current_time(void);

typedef struct _profile {
    struct _profile *hashlink;
    char *name;
   unsigned long long time;
   unsigned long long totaltime;
   unsigned long long starttime;
   unsigned long long globalstarttime;
    unsigned count;
    unsigned nesting;
} PROFILE;
    
#define PROFILESTR "$$$PROFILER"
#define PROFILESTR1 "$$$PROFILER1"

typedef struct _list
{
    struct _list *next;
    PROFILE *data;
} LIST;
static LIST *stack;
static int profiling = 0;
static int profileincomplete = 0;
static int profnames = 0;
static PROFILE *hashtab[HASH_SIZE];
static unsigned fnoverhead;
static int guard;

void _profile_in(char *name);
void _profile_out(char *name);
static PROFILE *HashIt(char *str);
static void validate(void);

static void init_profile(void)
{
    // won't work quite right in a multithreaded app
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL + THREAD_PRIORITY_HIGHEST);
    _current_time();
    profiling = 1;
    validate();
}

static int comparison(const void *a, const void *b)
{
    PROFILE *x=*((PROFILE **)a),*y=*((PROFILE **)b);
    unsigned r,s;
    r = x->totaltime;
    s = y->totaltime;
    if (r & 0xf0000000)
        r = 0;
    if (s & 0xf0000000)
        s = 0;
    if (r < s)
        return 1;
    if (r == s)
        return 0;
    return -1;
}
static void validate(void)
{
    unsigned v1,v2,v3;
    PROFILE *t,*t1;
    _profile_in(PROFILESTR);
    _profile_in(PROFILESTR1);
    _profile_out(PROFILESTR1);
    _profile_out(PROFILESTR);
    t = HashIt(PROFILESTR);
    fnoverhead = 0;//t->time;
}
static void show_profile(void)
{
    FILE *out;
    PROFILE **p;
#ifdef XXXXX
    LARGE_INTEGER x;
#endif
    int i,pos=0;
    char name[256],*s;
    unsigned long long totaltime = 0,totalcount=0;
    profiling = 0;
    guard++;
#ifdef XXXXX
    QueryPerformanceFrequency(&x);
#endif
    if (profileincomplete) {
        printf(stderr,"Not enough memory to perform profiling");
        return;
    }
    p = malloc(profnames * sizeof(PROFILE *));
    if (!p) {
        printf(stderr,"Can't allocate profile sort list");
        return;
    }
   strcpy(name,_argv[0]);
    s = strrchr(name,'.');
    if (!s)
        s = name + strlen(name);
    strcpy(s,".prf");
    out = fopen(name,"w");
    if (!out) {
        printf(stderr,"Can't open profile output file");
        return;
    }
    for (i=0; i < HASH_SIZE; i++) {
        PROFILE *q = hashtab[i];
        while (q) {
            totaltime += q->time;
            totalcount +=q->count;
            p[pos++] = q;
            q = q->hashlink;
        }
    }
    qsort(p,profnames,sizeof(PROFILE *),comparison);
    fprintf(out,"Profile info: (time: %10d, calls: %10d)\n\n",totaltime,totalcount);
    for (i=0; i < profnames; i++) {
        unsigned long long v = p[i]->time ;
        unsigned long long u = p[i]->totaltime;
        unsigned long long n = totaltime/100;
        fprintf(out,"%32s: %02u%%/%02u%% (time:%10u) (count:%10d) (avg:%10d)\n",p[i]->name,(int)(n > 0 ? v/n : 0),(int)( n >0 ? u/n : 0), (int)v,p[i]->count,v/p[i]->count);
    }
    fclose(out);
    
}
static unsigned int ComputeHash(char *string)
{
  unsigned rv = 0;
  while (*string)
      rv = (rv << 7) + (rv << 2) + rv + *string++;
  return rv % HASH_SIZE;
}
static PROFILE *HashIt(char *str)
{
    unsigned n = (unsigned)str;
    unsigned h = (n + (n >>8) + (n >> 16) + (n >> 24) + (n << 8) + (n << 16) + (n << 24)) % HASH_SIZE;
    PROFILE *p = hashtab[h];
    while (p) {
        if (p->name == str)
            return p;
        p = p->hashlink;
    }
    p = calloc(1, sizeof(PROFILE));
    if (!p)
        return p;
    profnames++;
    p->name = str;
    p->hashlink = hashtab[h];
    hashtab[h] = p;
    return p;
}
void _profile_in(char *name)
{
    LIST *l;
    long long t;
    unsigned long long x;
    PROFILE *p;
    if (guard || !profiling)
        return;
    guard++;
    x = _current_time();
    if (stack) {
        t = x - stack->data->starttime;
        if (t < 0) t= - t;
        stack->data->time += t - fnoverhead;
    }

    profiling = 0;
    p = HashIt(name);
    if (!p) {
        profileincomplete = 1;
        guard--;
        return;
    }
    l = calloc(1, sizeof(LIST));
    if (!l) {
        profileincomplete = 1;
        guard--;
        return;
    }
    l->data = p;
    l->next = stack;
    stack = l;
    profiling = 1;
    
    p->count++;
    p->starttime = x;
   if (!p->nesting++)
        p->globalstarttime = x;
    guard--;
}
void _profile_out_1(char *name)
{
    long long t;
    unsigned long long x;
    PROFILE *proclink;
    LIST *toFree;
    if (guard || !profiling || !stack)
    {
        return;
    }
    guard++;
    x = _current_time();
    proclink = stack->data;
    toFree = stack;
    stack = toFree->next;
    free(toFree);
    t = x - proclink->starttime;
    if (t < 0) t = - t;
    proclink->time += t - fnoverhead;
    if (!--proclink->nesting)
    {
        t = x - proclink->globalstarttime;
        if (t < 0) t = - t;
        proclink->totaltime += t - fnoverhead;
    }
    if (stack)
        stack->data->starttime = x;
    guard--;
}
