/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */


#include <xtensa/coreasm.h>
#include <xtensa/corebits.h>
#include <xtensa/config/system.h>
#include "freertos/xtensa_context.h"
#include "esp_debug_helpers.h"
#include "sdkconfig.h"
#include "soc/soc.h"
#include "soc/dport_reg.h"
#include "soc/rmt_reg.h"

#define L5_INTR_STACK_SIZE  24
#define L5_INTR_A2_OFFSET   0
#define L5_INTR_A3_OFFSET   4
#define L5_INTR_A4_OFFSET   8
#define L5_INTR_A5_OFFSET   12
#define L5_INTR_A6_OFFSET   16
#define L5_INTR_A7_OFFSET   20

    .data
_l5_intr_stack0:
    .space      L5_INTR_STACK_SIZE
_l5_intr_stack1:
    .space      L5_INTR_STACK_SIZE

_l5_counter:
	.space		4,1
    .global     _l5_counter

_l5_flags:
	.space		4,0xff
    .global     _l5_flags

/**
 * some structure as FASTRMI_DATA in ownled.c
 */
#define MAXIMAL_LINES 8
#define BYTES_PER_LINE (616*3)

#define DATA_COUNTER_OFFSET 0
#define DATA_SIZE_OFFSET 2
#define DATA_ADDRESS_OFFSET 4
#define DATA_MASK_OFFSET 8
#define DATA_INTMASK_OFFSET 12
#define DATA_INPUT_OFFSET 16
#define DATA_LENGTH 20

fastrmi_para:
	.space		DATA_LENGTH * MAXIMAL_LINES,0
fastrmi_para_end:
    .global     fastrmi_para

fastrmi_bytes:
	.space		BYTES_PER_LINE * MAXIMAL_LINES, 0
	 .global     fastrmi_bytes

/*
Interrupt , a high-priority interrupt, is used for several things:
- Dport access mediation
- Cache error panic handler
- Interrupt watchdog panic handler

*/

    .section .iram1,"ax"

    .global     xt_highint5
    .type       xt_highint5,@function
    .align      4

xt_highint5:

    /* This section is for dport access register protection */
    /* Allocate exception frame and save minimal context. */
    /* Because the interrupt cause code has protection that only
       allows one cpu to enter in the dport section of the L4
       interrupt at one time, there's no need to have two
       _l4_intr_stack for each cpu */


/**************************************************
 * save stack
 */

    /* get CORE_ID */
    getcoreid   a0
    beqz    a0, 2f
    /* current cpu is 1 */
    movi    a0, _l5_intr_stack1
    j       3f
2:
    /* current cpu is 0 */
    movi    a0, _l5_intr_stack0
3:
    /* Save A2, A3 so we can use those registers */
    s32i    a2, a0, L5_INTR_A2_OFFSET
    s32i    a3, a0, L5_INTR_A3_OFFSET
    s32i    a4, a0, L5_INTR_A4_OFFSET
    s32i    a5, a0, L5_INTR_A5_OFFSET
    s32i    a6, a0, L5_INTR_A6_OFFSET
    s32i    a7, a0, L5_INTR_A7_OFFSET

/**************************************************
 * you will find which channels have triggered an interrupt here,
 * then, you can post some event to RTOS queue to process the event.
 * later we will add a queue in the driver code.
 */

	movi	a0, DR_REG_RMT_BASE
	//read RMT interrupt status.
	l32i	a3,	a0, RMT_INT_ST_REG - DR_REG_RMT_BASE
	//clear RMT interrupt status.
	s32i	a3, a0, RMT_INT_CLR_REG - DR_REG_RMT_BASE

	movi	a0, _l5_flags
	s32i	a3, a0, 0

/**************************************************
 * increase counter to check interrupt calls
 */

	movi	a0, _l5_counter
	l32i	a2, a0, 0
	addi	a2, a2, 1
	s32i	a2, a0, 0


/**************************************************
 * copy fastrmi buffer into RMT buffer
 */
	// load low and high bits
	movi	a0, cs8812_high
	l32i	a6, a0, 0
	movi	a0, cs8812_low
	l32i	a5, a0, 0

	// load pointer to led data
	movi	a0, fastrmi_para

	beqz	a3, loop_end

loop_start:
	// get first threshold bit. If not set, do not start inner loop
	l32i	a2, a0, DATA_INTMASK_OFFSET
	and		a2,	a3, a2
	beqz	a2, loop_increment

inner_start:
	// increment the counter offset, if it is not equal the size
	// if equal, fill will one zero entry
	l16si	a2, a0, DATA_COUNTER_OFFSET
	l16si	a4,	a0, DATA_SIZE_OFFSET
	bge		a2, a4, loop_zero
	addi	a4,	a2, 1
	s16i	a4, a0, DATA_COUNTER_OFFSET

	// get data pointer and LED color byte
	l32i	a4, a0, DATA_INPUT_OFFSET
	add		a4, a2, a4
	l8ui	a2, a4, 0

	// get RMT pointer
	l32i	a4, a0, DATA_ADDRESS_OFFSET

	// fill 8 bits into RMT buffer
	bbci	a2, 7, loop0a
	s32i	a6, a4, 0
	j		loop0b
loop0a:
	s32i	a5, a4, 0
loop0b:

	bbci	a2, 6, loop1a
	s32i	a6, a4, 4
	j		loop1b
loop1a:
	s32i	a5, a4, 4
loop1b:

	bbci	a2, 5, loop2a
	s32i	a6, a4, 8
	j		loop2b
loop2a:
	s32i	a5, a4, 8
loop2b:

	bbci	a2, 4, loop3a
	s32i	a6, a4, 12
	j		loop3b
loop3a:
	s32i	a5, a4, 12
loop3b:

	bbci	a2, 3, loop4a
	s32i	a6, a4, 16
	j		loop4b
loop4a:
	s32i	a5, a4, 16
loop4b:

	bbci	a2, 2, loop5a
	s32i	a6, a4, 20
	j		loop5b
loop5a:
	s32i	a5, a4, 20
loop5b:

	bbci	a2, 1, loop6a
	s32i	a6, a4, 24
	j		loop6b
loop6a:
	s32i	a5, a4, 24
loop6b:

	bbci	a2, 0, loop7a
	s32i	a6, a4, 28
	j		loop7b
loop7a:
	s32i	a5, a4, 28
loop7b:

/* add RMT pointer by 32 */
	addi	a4, a4, 32
	s32i	a4, a0, DATA_ADDRESS_OFFSET

/* check if half block finished */
	l32i	a2, a0, DATA_MASK_OFFSET
	srli	a2, a2, 1
	bany	a2, a4, inner_start		// no? continue filling bytes

/* check if full block finished */
	l32i	a2, a0, DATA_MASK_OFFSET
	bany	a2, a4, loop_increment

/* if so increment RMT pointer by block size */
	sub		a4, a4, a2
	addi	a4, a4, -1
	s32i	a4, a0, DATA_ADDRESS_OFFSET
	j		loop_increment

/**
 * fill RMT mem with zero to stop RMT
 */
loop_zero:
	// get RMT pointer
	l32i	a4, a0, DATA_ADDRESS_OFFSET
	// write stop instruction into RMT buffer
	movi	a2, 0
	s32i	a2, a4, 0

loop_increment:
	// add offset to buffer
	addi	a0, a0, DATA_LENGTH

	movi	a2, fastrmi_para_end
	bne 	a2, a0, loop_start

loop_end:


/**************************************************
 * Done. Restore registers and return.
 */
    /* get CORE_ID */
    getcoreid   a0
    beqz    a0, 4f
    /* current cpu is 1 */
    movi    a0, _l5_intr_stack1
    j       5f
4:
    /* current cpu is 0 */
    movi    a0, _l5_intr_stack0
5:
    l32i    a2, a0, L5_INTR_A2_OFFSET
    l32i    a3, a0, L5_INTR_A3_OFFSET
    l32i    a4, a0, L5_INTR_A4_OFFSET
    l32i    a5, a0, L5_INTR_A5_OFFSET
    l32i    a6, a0, L5_INTR_A6_OFFSET
    l32i    a7, a0, L5_INTR_A7_OFFSET

    rsync
    rsr     a0, EXCSAVE_5                   /* restore a0 */
    rfi     5


/* The linker has no reason to link in this file; all symbols it exports are already defined
   (weakly!) in the default int handler. Define a symbol here so we can use it to have the 
   linker inspect this anyway. */

    .global ld_include_xt_highint5
ld_include_xt_highint5:
