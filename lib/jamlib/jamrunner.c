/*

The MIT License (MIT)
Copyright (c) 2017 Muthucumaru Maheswaran

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY O9F ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "jam.h"
#include "core.h"

#include <task.h>
#include <string.h>
#include "threadsem.h"
#include "jamdata.h"
#include "nvoid.h"
#include "mqtt.h"
#include "activity.h"


// Create the runtable that contains all the actid entries
// WARNING:: This table reuses memory based on an LRU scheme.
// It should not slip memory underneath an active allocation. That is
// we should have a situation where the memory is preempted from an
// allocation that is actively used by an activity.

// TODO: Ensure we have memory safety. That is memory is not preempted while
// while being used. It could be a catastropic error to have memory preempted that way.
//
runtable_t *runtable_new(void *jarg)
{
    int i;

    runtable_t *rtab = (runtable_t *)calloc(1, sizeof(runtable_t));
    rtab->rcount = 0;
    rtab->jarg = jarg;

    pthread_mutex_init(&(rtab->lock), NULL);
    rtab->entries = (runtableentry_t *)calloc(MAX_RUN_ENTRIES, sizeof(runtableentry_t));

    // Initialize the entries
    for (i = 0; i < MAX_RUN_ENTRIES; i++)
    {
        rtab->entries[i].accesstime = 0;
        rtab->entries[i].status = EMPTY;
    }

    return rtab;
}


runtableentry_t *runtable_find(runtable_t *table, char *actid)
{
    int i, j = -1;

    if (actid == NULL)
        return NULL;

    pthread_mutex_lock(&(table->lock));
    for(i = 0; i < MAX_RUN_ENTRIES; i++)
    {
        // Search through PRESENT and DELETED entries in the table
        // If the table entry matches the actid, then we FOUND
        if(table->entries[i].status != EMPTY)
            if(strcmp(actid, table->entries[i].actid) == 0)
            {
                j = i;
                break;
            }
    }
    // update the access time of the selected one
    if (j >= 0)
        table->entries[j].accesstime = activity_getseconds();

    pthread_mutex_unlock(&(table->lock));

    if (j < 0)
        return NULL;
    else
        return &(table->entries[j]);
}


runtableentry_t *runtable_getfree(runtable_t *table)
{
    int i, j = -1;
    long long minatime = activity_getseconds();

    pthread_mutex_lock(&(table->lock));
    for(i = 0; i < MAX_RUN_ENTRIES; i++)
    {
        // found an empty slot
        if(table->entries[i].status == EMPTY)
        {
            pthread_mutex_unlock(&(table->lock));
            return &(table->entries[i]);
        }
        else
        {
            // otherwise.. find the oldest entry among the deleted using FIFO
            if ((table->entries[i].status == DELETED) &&
                (minatime > table->entries[i].accesstime))
            {
                minatime = table->entries[i].accesstime;
                j = i;
            }
        }
    }
    pthread_mutex_unlock(&(table->lock));
    if (j < 0)
        return NULL;

    return &(table->entries[j]);
}


bool runtable_insert(jamstate_t * js, char *actid, command_t *cmd)
{
    // find the entry.. if found no insert
    runtableentry_t *re = runtable_find(js->rtable, actid);
    if (re != NULL)
        return false;

    // else get a free slot and insert the entry in that slot
    // TODO: FIX: We are searching the table twice..
    //
    re = runtable_getfree(js->rtable);
    if (re == NULL)
    {
        printf("Cannot get a free slot in runtable \n");
        exit(1);
    }

    strcpy(re->actid, actid);
    strcpy(re->actname, cmd->actname);

    re->accesstime = activity_getseconds();
    re->status = STARTED;

    pthread_mutex_lock(&(js->rtable->lock));
    js->rtable->rcount++;
    pthread_mutex_unlock(&(js->rtable->lock));

    return true;
}


bool runtable_del(runtable_t *tbl, char *actid)
{
    // find the entry.. if not found return with false
    runtableentry_t *re = runtable_find(tbl, actid);
    if (re == NULL)
        return false;

    // Memory held by the entry is still there.. we need it to check if
    // a callback is relevant for the local node.
    // We should not use too small a runtable.. otherwise, we could run into
    // race condition caused by premature eviction
    // We are just marking it as deleted.
    //

    pthread_mutex_lock(&(tbl->lock));
    re->status = DELETED;
    tbl->rcount--;
    pthread_mutex_unlock(&(tbl->lock));

    return true;
}


void jrun_arun_callback(jactivity_t *jact, command_t *cmd, activity_callback_reg_t *creg)
{
    // Activity to run the callback is already created..
    // No need to create it again...
    //

    #ifdef DEBUG_LVL1
    printf("========= >> Starting the function....................\n");
    #endif
    creg->cback(jact, cmd);

    // if the execution was done due to a remote request...
    if (jact->remote)
    {
        // Delete the activity.. because we are doing a remote processing..
        activity_free(jact);
    }

    // Don't free cmd here.. it should be freed in the calling function..
}
