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

#define MaxFileLen 32
#define MaxFileDescriptors 20

FileDescriptor fileDescriptorTable[MaxFileDescriptors];
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

// Hàm đảm bảo tăng thanh ghi PC để tiếp tục thực hiện chương trình
void IncreasePC() {
	int pc = kernel->machine->ReadRegister(PCReg);
	kernel->machine->WriteRegister(PrevPCReg, pc);
	kernel->machine->WriteRegister(PCReg, pc + 4);
	kernel->machine->WriteRegister(NextPCReg, pc + 8);
}

// Hàm sao chép một chuỗi từ user space vào kernel space 
bool copyStringFromUser(int userAddr, char* kernelBuf, int maxBytes) {
	int bytesRead = 0;
	int value;

	while (bytesRead < maxBytes - 1) { // Trừ 1 để dự trữ chỗ cho null-terminated character
		if (!machine->ReadMem(userAddr + bytesRead, 1, &value)) { // Dùng ReadMem
			// Lỗi khi đọc từ user space
			return false;
		}

		kernelBuf[bytesRead] = (char)value;

		if (kernelBuf[bytesRead] == '\0') {
			// Kết thúc chuỗi null-terminated
			return true;
		}

		bytesRead++;
	}

	// Đảm bảo kết thúc chuỗi bằng null-terminated nếu không đủ không gian trong kernelBuf
	kernelBuf[bytesRead] = '\0';

	return true;
}



// 
void
ExceptionHandler(ExceptionType which) {
    int type = kernel->machine->ReadRegister(2);

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

				// Create file
				case CS_Create:
					int userFilenameAddr = kernel->machine->ReadRegister(4); // Lấy địa chỉ file
					char filename[MaxFileLen];

					if (!copyStringFromUser(userFilenameAddr, filename, MaxFileLen)) {
						// Nếu sao chép dữ liệu từ user space thất bại
						kernel->machine->WriteRegister(2, -1);
						break;
					}

					// -> tạo file từ Nachos FileSystem Object
					OpenFile* newFile = fileSystem->Create(filename);

					if (newFile != NULL) {
						kernel->machine->WriteRegister(2, 0); // Thành công: 0
					}
					else {
						kernel->machine->WriteRegister(2, -1); // Lỗi: -1
					}
						
					IncreasePC();
					break;
				
				// Open file
				case SC_Open:
					int userFilenameAddr = kernel->machine->ReadRegister(4);
					int fileType = kernel->machine->ReadRegister(5);
					char filename[MaxFileLen];

					if (!copyStringFromUser(userFilenameAddr, filename, MaxFileLen)) {
						kernel->machine->WriteRegister(2, -1); // Lỗi khi sao chép tên file
						break;
					}

					// Check type của file
					if (fileType != 0 && fileType != 1) {
						kernel->machine->WriteRegister(2, -1); // Lỗi: Type không hợp lệ
						break;
					}

					// Mở file từ Nachos FileSystem Object
					OpenFile* openedFile = fileSystem->Open(filename);

					if (openedFile != NULL) {
						// Tìm một vị trí trống trong bảng file descriptor
						int fileDescriptor = -1;
						for (int i = 2; i < MaxFileDescriptors; i++) {
							if (fileDescriptorTable[i].file == NULL) {
								fileDescriptor = i;
								break;
							}
						}

						if (fileDescriptor == -1) {
							kernel->machine->WriteRegister(2, -1); // Không còn chỗ trống trong bảng
						}
						else {
							fileDescriptorTable[fileDescriptor].file = openedFile;
							fileDescriptorTable[fileDescriptor].type = fileType;
							kernel->machine->WriteRegister(2, fileDescriptor);
						}
					}
					else {
						kernel->machine->WriteRegister(2, -1); // Lỗi khi mở file
					}

					IncreasePC();
					break;

				// Close file
				case SC_Close:
					int fileDescriptor = kernel->machine->ReadRegister(4);

					if (fileDescriptor >= 2 && fileDescriptor < MaxFileDescriptors && fileDescriptorTable[fileDescriptor].file != NULL) {
						delete fileDescriptorTable[fileDescriptor].file;
						fileDescriptorTable[fileDescriptor].file = NULL;
						fileDescriptorTable[fileDescriptor].type = -1;
						kernel->machine->WriteRegister(2, 0); // Thành công : 0
					}
					else {
						kernel->machine->WriteRegister(2, -1); // Lỗi : -1
					}

					IncreasePC();
					break;

				//
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
