/*
The MIT License (MIT)
Copyright (c) 2016 Muthucumaru Maheswaran

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:
The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __ACTIVITY_H__
#define __ACTIVITY_H__

#include "nvoid.h"
#include "task.h"
#include "command.h"
#include "simplequeue.h"
#include "simplelist.h"
#include "pushqueue.h"


#include <stdbool.h>
#include <stdint.h>

#define MAX_NAME_LEN            64
#define MAX_ACT_THREADS         32
#define MAX_CALLBACKS           16 //32
#define MAX_FIELD_LEN           64

typedef void (*activitycallback_f)(void *ten, void *arg);

enum activity_state_t
{
    EMPTY,
    DELETED,
    COMPLETED,
    NEW,
    STARTED,
    NEGATIVE_COND,
    PARAMETER_ERROR,
    TIMEDOUT,
    PARTIAL,
    FATAL_ERROR,
    ABORTED
};


enum activity_type_t
{
    SYNC,
    ASYNC,
    SYNC_RTE,
    SYNC_NRT
};

typedef struct _activity_thread_t
{
    int jindx;
    // The integer wraparound is not a concern. We are bound to restart
    // The C node before there is a wraparound.
    int threadid;
    int taskid;

    pushqueue_t *inq;
    simplequeue_t *outq;
    pushqueue_t *resultq;

} activity_thread_t;


typedef struct _activity_callback_reg_t
{
    char name[MAX_NAME_LEN];
    char signature[MAX_NAME_LEN];
    activitycallback_f cback;

    enum activity_type_t type;

} activity_callback_reg_t;


typedef struct _activity_table_t
{
    // This is a parent pointer to the jamstate_t
    // We are holding the void pointer to avoid type issues..
    void *jarg;

    int jcounter;
    int numcbackregs;
    // Callbacks are NOT pre-initialized..
    activity_callback_reg_t *callbackregs[MAX_CALLBACKS];

    // Pre-initialize the activity threads..
    activity_thread_t *athreads[MAX_ACT_THREADS];

    simplequeue_t *globaloutq;
    push2queue_t *globalinq;

    list_elem_t *alist;

    pthread_mutex_t lock;

} activity_table_t;


typedef struct _jactivity_t
{
    enum activity_type_t type;

    char actid[MAX_FIELD_LEN];
    int jindx;

    long long accesstime;
    bool remote;
    activity_table_t *atable;

} jactivity_t;



//
// Function prototypes..
//

char *activity_gettime(char *prefix);
long long activity_getseconds();
long activity_getuseconds();

activity_table_t *activity_table_new(void *arg);
void activity_table_print(activity_table_t *at);
void activity_callbackreg_print(activity_callback_reg_t *areg);
void activity_printthread(activity_thread_t *ja);

bool activity_regcallback(activity_table_t *at, char *name, int type, char *sig, activitycallback_f cback);
activity_callback_reg_t *activity_findcallback(activity_table_t *at, char *name);

void run_activity(void *arg);

activity_thread_t *athread_init(activity_table_t *atbl);

activity_thread_t *athread_getmine(activity_table_t *at);
activity_thread_t *athread_get(activity_table_t *at, int jindx);

jactivity_t *activity_new(activity_table_t *at, char *actid, bool remote);
jactivity_t *activity_renew(activity_table_t *at, jactivity_t *jact);

void activity_free(jactivity_t *jact);
activity_thread_t *athread_getbyindx(activity_table_t *at, int jindx);
activity_thread_t *athread_getbyid(activity_table_t *at, char *actid);
jactivity_t *activity_getbyid(activity_table_t *at, char *actid);
jactivity_t *activity_getbyindx(activity_table_t *at, int jindx);
void activity_complete(activity_table_t *at, char *opt, char *actid, char *fmt, ...);

#endif
