// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------
void 
SyscallYield() {
        AddrSpace *currentAddrSpace = currentThread->space;
        pcb *currentProcess = currentAddrSpace->PCB;
        printf("System Call: %d invoked Yield", currentAddrSpace->PCB->processID);
        currentThread->Yield();
}

void 
SyscallExit() {
 	AddrSpace *currentAddrSpace = currentThread->space;
 	pcb *currentPCB = currentAddrSpace->PCB;
 	int input = machine->ReadRegister(4);

        printf("System Call: %d invoked Exit", currentAddrSpace->PCB->processID);
	
 	currentPCB->setParentToNull();
	currentPCB->removeChild();
        currentAddrSpace->PCB->clearID();
	currentAddrSpace->ReleaseMemory();
 	currentThread->Finish();
 }

void 
SyscallFork() {
 	AddrSpace *currentAddrSpace = currentThread->space;
 	pcb *currentPCB = currentAddrSpace->PCB;
        printf("System Call: %d invoked Fork", currentAddrSpace->PCB->processID);
  }

void 
SyscallExec() {
 	AddrSpace *currentAddrSpace = currentThread->space;
 	pcb *currentPCB = currentAddrSpace->PCB;
        printf("System Call: %d invoked Exec", currentAddrSpace->PCB->processID);
 }

void 
SyscallJoin() {
 	AddrSpace *currentAddrSpace = currentThread->space;
	pcb *currentPCB = currentAddrSpace->PCB;
        printf("System Call: %d invoked Join", currentAddrSpace->PCB->processID);
  }

void 
SyscallKill() {
 	AddrSpace *currentAddrSpace = currentThread->space;
 	pcb *currentPCB = currentAddrSpace->PCB;
        printf("System Call: %d invoked Kill", currentAddrSpace->PCB->processID);
  }

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
/*
    if ((which == SyscallException) && (type == SC_Halt)) {
	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();
    } else {
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
    }*/

    switch (which)
    {
    	case SyscallException:
    		switch(type)
    		{
    			case SC_Halt:	//moved previous code into switch case
    			{
	    			DEBUG('a', "Shutdown, initiated by user program.\n");
	   				interrupt->Halt();
	   				break;
   				}

   				case SC_Exit:
   				{
   					DEBUG('a', "Requesting exit syscall.\n");
   					SyscallExit();
					break;
   				}

   				case SC_Exec:
   				{
   					DEBUG('a', "Requesting exec syscall.\n");
   					SyscallExec();
					break;
   				}

   				case SC_Join:
   				{
   					DEBUG('a', "Requesting join syscall.\n");
   					SyscallJoin();
					break;
   				}

   				case SC_Open:
   				{
   					DEBUG('a', "Requesting open syscall.\n");
   					break;
   				}

   				case SC_Read:
   				{
   					DEBUG('a', "Requesting read syscall.\n");
   					break;
   				}

   				case SC_Write:
   				{
   					DEBUG('a', "Requesting write syscall.\n");
   					break;
   				}

   				case SC_Close:
   				{
   					DEBUG('a', "Requesting close syscall.\n");
   					break;
   				}

   				case SC_Fork:
   				{
   					DEBUG('a', "Requesting fork syscall.\n");
   					SyscallFork();
					break;
   				}

   				case SC_Yield:
   				{
            				DEBUG('a', "Requesting a yield syscall.\n");
            				SyscallYield();
            				break;
   				}

          default: ASSERT(FALSE);

          IncrementPC();
    	}


    }
}
