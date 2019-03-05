// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#ifdef HOST_SPARC
#include <strings.h>
#endif

void AddrSpace::Translation(int virtualAddr, int * physicalAddr, int size){
	DEBUG('s',"Entering Translation function %x, size %d = ", virtualAddr, size);
	unsigned int index, offset, pageFrame;

	index = (unsigned)virtualAddr / PageSize;
	offset = (unsigned)virtualAddr % PageSize;
	
	if (index >= numPages || index <= 0) {
		DEBUG('s', "Virtual page %d cannot be translated.\n", virtualAddr);
	return;
	}
	else if(!pageTable[index].valid) {
		DEBUG('s',"Virtual page %d cannot be translated because page isn't valid.\n", virtualAddr);
	return;
	}

	pageFrame = pageTable[index].physicalPage;
	if (pageFrame >= NumPhysPages) {
		DEBUG('s', "Virtual page %d cannot be translated because pageFrame: %d > the number of physical pages.\n", pageFrame);
	return;
	}

	pageTable[index].use = TRUE;
	*physicalAddr = pageFrame * PageSize + offset;
	ASSERT((*physicalAddr >= 0) && ((*physicalAddr + size) <= MemorySize));
	DEBUG('s', "Physical Addr 0x%x\n", *physicalAddr);
}

//---------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//--------------------------------------------------------------------
static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

// empty constructor for fork 
AddrSpace::AddrSpace(){

}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;
    unsigned int i, size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    ASSERT(numPages <= memoryManager->getNumFreePages()); //check there is space	

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);
// first, set up the translation 
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
	pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
	pageTable[i].physicalPage = memoryManager->getPage();
	pageTable[i].valid = TRUE;
	pageTable[i].use = FALSE;
	pageTable[i].dirty = FALSE;
	pageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only
	bzero(machine->mainMemory + pageTable[i].physicalPage*PageSize, PageSize);
    }
   
// then, copy in the code and data segments into memory
    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);       

        LoadCode(noffH.code.virtualAddr, noffH.code.size, noffH.code.inFileAddr, executable);
   }

    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
	
        LoadCode(noffH.initData.virtualAddr, noffH.initData.size, noffH.initData.inFileAddr, executable);
    }

}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
   delete pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}

void AddrSpace::LoadCode(int virtualAddr, int size, int fileAddr, OpenFile* file){
        int currentVirtualAddr = virtualAddr;
        int physicalAddr = 0;
        int codeSize = size;
        char *temp = new char[size];
        int currentTemp = 0;
        file->ReadAt(temp, size, fileAddr);

	printf("Loaded Program: [%d] code | [y] data | [z] bss. \n", codeSize);
	
	if (virtualAddr % PageSize != 0) { //if we don't cover full page
		int minSize = min(size, PageSize - virtualAddr % PageSize);
		Translation(currentVirtualAddr, &physicalAddr, minSize);
		bcopy(temp + currentTemp, machine->mainMemory + physicalAddr, minSize);
		codeSize -= minSize;
		currentTemp += minSize;
		currentVirtualAddr += minSize;
	}

	while(codeSize >= PageSize){ // while there is still code in file 
		Translation(currentVirtualAddr, &physicalAddr, PageSize);
		bcopy(temp + currentTemp, machine->mainMemory + physicalAddr, PageSize); //Load data into new address
		codeSize -= PageSize;
		currentTemp += PageSize;
		currentVirtualAddr += PageSize;
	}

	if (codeSize != 0) { //if we have code left on last page
		Translation(currentVirtualAddr, &physicalAddr, codeSize);
		bcopy(temp + currentTemp, machine->mainMemory + physicalAddr, codeSize);
		currentTemp += codeSize;
		currentVirtualAddr += codeSize;
		}
	
	ASSERT(currentTemp == size); //check we have traverse entire code
	delete temp;
}

void AddrSpace::ReleaseMemory(){
	for (int i = 0; i < numPages; i++) {
		memoryManager->clearPage(pageTable[i].physicalPage);
	}
}

bool AddrSpace::ReplaceContent(OpenFile *executable){
 NoffHeader noffH;
    unsigned int i, size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) &&
                (WordToHost(noffH.noffMagic) == NOFFMAGIC))
        SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size
                        + UserStackSize;        // we need to increase the size
                                                // to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    ASSERT(numPages <= memoryManager->getNumFreePages()); //check there is space        

    DEBUG('a', "Initializing address space, num pages %d, size %d\n",
                                        numPages, size);
// first, set up the translation 
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
        pageTable[i].virtualPage = i;   // for now, virtual page # = phys page #
        pageTable[i].physicalPage = memoryManager->getPage();
        pageTable[i].valid = TRUE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
                                        // a separate page, we could set its 
                                        // pages to be read-only
        bzero(machine->mainMemory + pageTable[i].physicalPage*PageSize, PageSize);
    }

// then, copy in the code and data segments into memory
    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n",
                        noffH.code.virtualAddr, noffH.code.size);

        LoadCode(noffH.code.virtualAddr, noffH.code.size, noffH.code.inFileAddr, executable);
    }

    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n",
                        noffH.initData.virtualAddr, noffH.initData.size);

        LoadCode(noffH.initData.virtualAddr, noffH.initData.size, noffH.initData.inFileAddr, executable);
    }

}

AddrSpace* AddrSpace::Copy(){
	AddrSpace *copy = new AddrSpace();
	unsigned int i;
	printf("Process [%d] Fork: start at address [d] with [%d] pages memory.\n", this->PCB->processID, numPages);

	if (memoryManager->getNumFreePages() < numPages){
		DEBUG('s', "Not enough pages for the forked process");
	return NULL;
	}
	
	copy->pageTable = new TranslationEntry[numPages];
	copy->numPages = numPages;

    	for (i = 0; i < numPages; i++) {
		copy->pageTable[i].virtualPage = pageTable[i].virtualPage;	
		copy->pageTable[i].physicalPage = pageTable[i].physicalPage;
		copy->pageTable[i].valid = pageTable[i].valid;
		copy->pageTable[i].use = pageTable[i].use;
		copy->pageTable[i].dirty = pageTable[i].dirty;
		copy->pageTable[i].readOnly = pageTable[i].readOnly;
		bcopy(machine->mainMemory + pageTable[i].physicalPage*PageSize, machine->mainMemory + copy->pageTable[i].physicalPage*PageSize, PageSize);
	}

return copy;
}

void AddrSpace::RestoreRegisters(){
        for (int i = 0; i < NumTotalRegs; i++){
                machine->WriteRegister(i, tempRegs[i]);
        }
}

void AddrSpace::SaveRegisters(){
	for (int i = 0; i < NumTotalRegs; i++){
		tempRegs[i] = machine->ReadRegister(i);
	}

}
