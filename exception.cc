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
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "syscall.h"
#include "ksyscall.h"
#include "filesys.h"

#define MaxFileLength 32
#define MaxFileDescriptors 20

int fileDescriptorTable[MaxFileDescriptors];
OpenFile *fileTable[MaxFileDescriptors];

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
// If you are handling a system call, don't forget to increment the pc
// before returning. (Or else you'll loop making the same system call forever!)
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	is in machine.h.
//----------------------------------------------------------------------

//
void IncreasePC()
{
	int counter = kernel->machine->ReadRegister(PCReg);
   	kernel->machine->WriteRegister(PrevPCReg, counter);
    counter = kernel->machine->ReadRegister(NextPCReg);
    kernel->machine->WriteRegister(PCReg, counter);
   	kernel->machine->WriteRegister(NextPCReg, counter + 4);
}

//
char* User2System(int virtAddr,int limit) { 
	int i;// index 
	int oneChar; 
	char* kernelBuf = NULL; 
	kernelBuf = new char[limit +1];//need for terminal string 
	if (kernelBuf == NULL) 
	return kernelBuf; 
	memset(kernelBuf,0,limit+1); 
	//printf("\n Filename u2s:"); 
	for (i = 0 ; i < limit ;i++) 
	{ 
	kernel->machine->ReadMem(virtAddr+i,1,&oneChar); 
	kernelBuf[i] = (char)oneChar; 
	//printf("%c",kernelBuf[i]); 
	if (oneChar == 0) 
	break; 
	} 
	return kernelBuf; 
} 

//
int System2User(int virtAddr,int len,char* buffer) { 
	if (len < 0) return -1; 
	if (len == 0)return len; 
	int i = 0; 
	int oneChar = 0 ; 
	do{ 
	oneChar= (int) buffer[i]; 
	kernel->machine->WriteMem(virtAddr+i,1,oneChar); 
	i ++; 
	}while(i < len && oneChar != 0); 
	return i; 
}


void ExceptionHandler(ExceptionType which) {
    int type;
	type = kernel->machine->ReadRegister(2);

    DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");
	

    switch (which) {
    case SyscallException:
      	switch(type) {
			//Halt
			case SC_Halt:
				DEBUG(dbgSys, "Shutdown, initiated by user program.\n");
				SysHalt();

				ASSERTNOTREACHED();
				break;
			//Read string 
			/*case SC_ReadString:
				int virtAddr, length;
				char* buffer;
				virtAddr = kernel->machine->ReadRegister(4); 
				length = kernel->machine->ReadRegister(5); 
				buffer = User2System(virtAddr, length); 
				gSynchConsole->Read(buffer, length); 
				System2User(virtAddr, length, buffer);
				delete buffer; 
				IncreasePC(); 
				return;
				break;

			//Print string
			case SC_PrintString:
				int virtAddr;
				char* buffer = new char[255];
				virtAddr = kernel->machine->ReadRegister(4); 
				buffer = User2System(virtAddr, 255); 
				int length = 0;
					while (buffer[length] != 0){
						gSynchConsole->Write(buffer + length, 1);
						length++;
					}
				buffer[length] = '\n';
				gSynchConsole->Write(buffer + length, 1);
				delete[] buffer; 
				break;
            
			//Print char
			case SC_PrintChar:
				char c = (char)kernel->machine->ReadRegister(4); 
				gSynchConsole->Write(&c, 1); 
				break;*/
			
			//Add
			case SC_Add:
				DEBUG(dbgSys, "Add " << kernel->machine->ReadRegister(4) << " + " << kernel->machine->ReadRegister(5) << "\n");
				
				/* Process SysAdd Systemcall*/
				int result;
				result = SysAdd(/* int op1 */(int)kernel->machine->ReadRegister(4),
						/* int op2 */(int)kernel->machine->ReadRegister(5));

				DEBUG(dbgSys, "Add returning with " << result << "\n");
				/* Prepare Result */
				kernel->machine->WriteRegister(2, (int)result);
				
				/* Modify return point */
				{
				/* set previous programm counter (debugging only)*/
				kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

				/* set programm counter to next instruction (all Instructions are 4 byte wide)*/
				kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
				
				/* set next programm counter for brach execution */
				kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
				}

				return;
		
				ASSERTNOTREACHED();
				break;

			//Create file
			case SC_CreateFile:
				int virtAddr; 
				char* filename; 
				DEBUG('a',"\n SC_Create call ..."); 
				DEBUG('a',"\n Reading virtual address of filename"); 
				virtAddr = kernel->machine->ReadRegister(4); 
				DEBUG ('a',"\n Reading filename."); 
				filename = User2System(virtAddr, MaxFileLength+1); 
				if (filename == NULL) { 
					printf("\n Not enough memory in system"); 
					DEBUG('a',"\n Not enough memory in system"); 
					kernel->machine->WriteRegister(2,-1); 
					delete filename; 
					return; 
				} 
				DEBUG('a',"\n Finish reading filename."); 	

				if (!FileSystem->Create(filename, 0)) { 
					printf("\n Error create file '%s'",filename); 
					kernel->machine->WriteRegister(2, -1); 
					delete filename; 
					return; 
				}
				
				kernel->machine->WriteRegister(2, 0);
				delete filename; 
				IncreasePC();
				ASSERTNOTREACHED();
				break;  

			/*//Open file
			case SC_Open:
				int virtAddr2;
				virtAddr2 = kernel->machine->ReadRegister(4);
				int fileType;
				fileType= kernel->machine->ReadRegister(5);
				char* filename2;
				filename2 = User2System(virtAddr2, MaxFileLength);

				if (fileType != 0 || fileType != 1) {
					kernel->machine->WriteRegister(2, -1);
					break;
				}

				OpenFile* openedFile = FileSystem->Open(filename2);
				if (openedFile != NULL) {
					int fileDescriptor = -1;
					for (int i = 2; i < MaxFileDescriptors; i++) {
						if (fileTable[i] == NULL) {
							fileDescriptor = i;
							break;
						}
					}
					if (fileDescriptor == -1) {
						kernel->machine->WriteRegister(2, -1); 
					}
					else {
						fileTable[fileDescriptor] = openedFile;
						fileDescriptorTable[fileDescriptor] = fileType;
						kernel->machine->WriteRegister(2, fileDescriptor);
					}
				}
				else {
					kernel->machine->WriteRegister(2, -1);
				}

				IncreasePC();
				break;
			
			//Close file
			case SC_Close:
				int fileDescriptor;
				fileDescriptor = kernel->machine->ReadRegister(4);

				if (fileDescriptor >= 2 && fileDescriptor < MaxFileDescriptors && fileTable[fileDescriptor] != NULL) {
					delete fileTable[fileDescriptor];
					fileTable[fileDescriptor] = NULL;
					fileDescriptorTable[fileDescriptor] = -1;
					kernel->machine->WriteRegister(2, 0); 
				}
				else {
					kernel->machine->WriteRegister(2, -1); 
				}

				break;*/
				
			default:
				cerr << "Unexpected system call " << type << "\n";
				break;
       	}
      	break;

    default:
      cerr << "Unexpected user mode exception" << (int)which << "\n";
      break;
    }
    ASSERTNOTREACHED();
}
