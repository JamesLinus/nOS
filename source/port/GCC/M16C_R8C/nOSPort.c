/*
 * Copyright (c) 2014-2015 Jim Tremblay
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define NOS_PRIVATE
#include "nOS.h"

#ifdef __cplusplus
extern "C" {
#endif

void nOS_InitContext(nOS_Thread *thread, nOS_Stack *stack, size_t ssize, nOS_ThreadEntry entry, void *arg)
{
    nOS_Stack *tos = (nOS_Stack*)((uint16_t)(stack + ssize) & 0xFFFE);
#if (NOS_CONFIG_DEBUG > 0)
    size_t i;

    for (i = 0; i < ssize; i++) {
        stack[i] = 0xFFFF;
    }
#endif

    /* FLG = 0x0C0 = interrupts enabled, USP stack pointer */
    *(--tos) = 0x00C0;
    *(--tos) = (nOS_Stack)entry;

#if (NOS_CONFIG_DEBUG > 0)
    *(--tos) = 0xFFFF;         /* FB */
    *(--tos) = 0xCCCC;         /* SB */
    *(--tos) = 0xBBBB;         /* A1 */
    *(--tos) = 0xAAAA;         /* A0 */
    *(--tos) = 0x3333;         /* R3 */
    *(--tos) = 0x2222;         /* R2 */
#else
        tos -= 6;              /* FB, SB, A1, A0, R3, R2 */
#endif
    *(--tos) = (nOS_Stack)arg; /* R1 */
#if (NOS_CONFIG_DEBUG > 0)
    *(--tos) = 0x1111;         /* R0 */
#else
        tos -= 1;              /* R0 */
#endif

    thread->stackPtr = tos;
}

void nOS_EnterIsr (void)
{
    nOS_EnterCritical();
    nOS_isrNestingCounter++;
    nOS_LeaveCritical();
}

bool nOS_LeaveIsr (void)
{
    bool    swctx = false;

    nOS_EnterCritical();
    nOS_isrNestingCounter--;
#if (NOS_CONFIG_SCHED_PREEMPTIVE_ENABLE > 0)
    if (nOS_isrNestingCounter == 0) {
 #if (NOS_CONFIG_SCHED_LOCK_ENABLE > 0)
        if (nOS_lockNestingCounter == 0)
 #endif
        {
            nOS_highPrioThread = nOS_FindHighPrioThread();
            if (nOS_runningThread != nOS_highPrioThread) {
                swctx = true;
            }
        }
    }
#endif
    nOS_LeaveCritical();

    return swctx;
}

void nOS_SwitchContextHandler(void)
{
    __asm volatile (
        /* Push all registers on thread stack */
        "PUSHM  R0,R1,R2,R3,A0,A1,SB,FB                     \n"

        /* Save SP to nOS_Thread object of current running thread */
        "MOV.W  _nOS_runningThread,     A0                  \n"
        "STC    SP,                     [A0]                \n"

        /* Copy nOS_highPrioThread to nOS_runningThread */
        "MOV.W  _nOS_highPrioThread,    _nOS_runningThread  \n"

        /* Restore SP from nOS_Thread object of high prio thread */
        "MOV.W  _nOS_highPrioThread,    A0                  \n"
        "LDC    [A0],                   SP                  \n"

        /* Pop all registers from thread stack */
        "POPM   R0,R1,R2,R3,A0,A1,SB,FB                     \n"
    );
}

void nOS_SwitchContextFromIsrHandler(void)
{
    __asm volatile (
        /* Push all registers on thread stack */
        "PUSHM  R0,R1,R2,R3,A0,A1,SB,FB                     \n"

        /* Move PC and FLG from ISTACK to USTACK */
        "STC    ISP,                    A0                  \n"
        "MOV.W  0[A0],                  16[SP]              \n"
        "MOV.W  2[A0],                  18[SP]              \n"

        /* Adjust ISTACK (remove PC and FLG) */
        "ADD.W  #4,                     A0                  \n"
        "LDC    A0,                     ISP                 \n"

        /* Save SP to nOS_Thread object of current running thread */
        "MOV.W  _nOS_runningThread,     A0                  \n"
        "STC    SP,                     [A0]                \n"

        /* Copy nOS_highPrioThread to nOS_runningThread */
        "MOV.W  _nOS_highPrioThread,    _nOS_runningThread  \n"

        /* Restore SP from nOS_Thread object of high prio thread */
        "MOV.W  _nOS_highPrioThread,    A0                  \n"
        "LDC    [A0],                   SP                  \n"

        /* Pop all registers from thread stack */
        "POPM   R0,R1,R2,R3,A0,A1,SB,FB                     \n"
    );
}

#ifdef __cplusplus
}
#endif
