/*	$OpenBSD: db_machdep.h,v 1.4 2005/01/10 21:20:52 espie Exp $	*/
/*	$NetBSD: db_machdep.h,v 1.2 2003/04/29 17:06:04 scw Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#ifndef	_I386_DB_MACHDEP_H_
#define	_I386_DB_MACHDEP_H_

/*
 * Machine-dependent defines for new kernel debugger.
 */

#include <sys/param.h>
#include <uvm/uvm_extern.h>
#include <machine/trap.h>
#include <sys/mutex.h>

typedef	vaddr_t		db_addr_t;	/* address - unsigned */
typedef	long		db_expr_t;	/* expression - signed */

typedef struct trapframe db_regs_t;

extern db_regs_t ddb_regs;	/* register state */
#define	DDB_REGS	(&ddb_regs)

#define	PC_REGS(regs)	((db_addr_t)(regs)->tf_rip)
#define	SET_PC_REGS(regs, value) (regs)->tf_rip = (int64_t)(value)

#define	BKPT_ADDR(addr)	(addr)		/* breakpoint address */
#define	BKPT_INST	0xcc		/* breakpoint instruction */
#define	BKPT_SIZE	(1)		/* size of breakpoint inst */
#define	BKPT_SET(inst)	(BKPT_INST)

#define	FIXUP_PC_AFTER_BREAK(regs)	((regs)->tf_rip -= BKPT_SIZE)

#define	db_clear_single_step(regs)	((regs)->tf_rflags &= ~PSL_T)
#define	db_set_single_step(regs)	((regs)->tf_rflags |=  PSL_T)

#define	IS_BREAKPOINT_TRAP(type, code)	((type) == T_BPTFLT)
#define IS_WATCHPOINT_TRAP(type, code)	((type) == T_TRCTRAP && (code) & 15)

#define	I_CALL		0xe8
#define	I_CALLI		0xff
#define	I_RET		0xc3
#define	I_IRET		0xcf

#define	inst_trap_return(ins)	(((ins)&0xff) == I_IRET)
#define	inst_return(ins)	(((ins)&0xff) == I_RET)
#define	inst_call(ins)		(((ins)&0xff) == I_CALL || \
				 (((ins)&0xff) == I_CALLI && \
				  ((ins)&0x3800) == 0x1000))
#define inst_load(ins)		0
#define inst_store(ins)		0

/* access capability and access macros */

#define DB_ACCESS_LEVEL		2	/* access any space */
#define DB_CHECK_ACCESS(addr,size,task)				\
	db_check_access(addr,size,task)
#define DB_PHYS_EQ(task1,addr1,task2,addr2)			\
	db_phys_eq(task1,addr1,task2,addr2)
#define DB_VALID_KERN_ADDR(addr)				\
	((addr) >= VM_MIN_KERNEL_ADDRESS && 			\
	 (addr) < VM_MAX_KERNEL_ADDRESS)
#define DB_VALID_ADDRESS(addr,user)				\
	((!(user) && DB_VALID_KERN_ADDR(addr)) ||		\
	 ((user) && (addr) < VM_MAX_ADDRESS))

#if 0
boolean_t 	db_check_access(vaddr_t, int, task_t);
boolean_t	db_phys_eq(task_t, vaddr_t, task_t, vaddr_t);
#endif

/* macros for printing OS server dependent task name */

#define DB_TASK_NAME(task)	db_task_name(task)
#define DB_TASK_NAME_TITLE	"COMMAND                "
#define DB_TASK_NAME_LEN	23
#define DB_NULL_TASK_NAME	"?                      "

/*
 * Constants for KGDB.
 */
typedef	long		kgdb_reg_t;
#define	KGDB_NUMREGS	16
#define	KGDB_BUFLEN	512

#if 0
void		db_task_name(/* task_t */);
#endif

/* macro for checking if a thread has used floating-point */

#define db_thread_fp_used(thread)	((thread)->pcb->ims.ifps != 0)

int kdb_trap(int, int, db_regs_t *);

void db_machine_init(void);
int db_enter_ddb(void);
void db_startcpu(int cpu);
void db_stopcpu(int cpu);
void x86_ipi_db(struct cpu_info *);

extern struct mutex ddb_mp_mutex;

#define DDB_STATE_NOT_RUNNING	0
#define DDB_STATE_RUNNING	1
#define DDB_STATE_EXITING	2

/*
 * We define some of our own commands
 */
#define DB_MACHINE_COMMANDS

#define	DB_ELF_SYMBOLS
#define	DB_ELFSIZE	64

extern void db_machine_init(void);

extern void cpu_debug_dump(void);

#endif	/* _I386_DB_MACHDEP_H_ */
