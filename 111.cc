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

// Input: - User space address (int) 
//  - Limit of buffer (int) 
// Output:- Buffer (char*) 
// Purpose: Copy buffer from User memory space to System memory space 
char* User2System(int virtAddr, int limit)
{
	int i;// index 
	int oneChar;
	char* kernelBuf = NULL;
	kernelBuf = new char[limit + 1];//need for terminal string 
	if (kernelBuf == NULL)
		return kernelBuf;
	memset(kernelBuf, 0, limit + 1);
	//printf("\n Filename u2s:"); 
	for (i = 0; i < limit; i++)
	{
		machine->ReadMem(virtAddr + i, 1, &oneChar);
		kernelBuf[i] = (char)oneChar;
		//printf("%c",kernelBuf[i]); 
		if (oneChar == 0)
			break;
	}
	return kernelBuf;
}
// Input: - User space address (int) 
//       - Limit of buffer (int) 
//       - Buffer (char[]) 
// Output:- Number of bytes copied (int) 
// Purpose: Copy buffer from System memory space to User  memory space 
int   System2User(int virtAddr, int len, char* buffer)
{
	if (len < 0) return -1;
	if (len == 0)return len;
	int i = 0;
	int oneChar = 0;
	do {
		oneChar = (int)buffer[i];
		machine->WriteMem(virtAddr + i, 1, oneChar);
		i++;
	} while (i < len && oneChar != 0);
	return i;
}

void ExceptionHandler(ExceptionType which)
{
    int type = kernel->machine->ReadRegister(2);

    DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");

    switch (which) {
	case NoException:
		return;
    case SyscallException:
      switch(type) { 
      /*case SC_Halt:
	DEBUG(dbgSys, "Shutdown, initiated by user program.\n");

	SysHalt();

	ASSERTNOTREACHED();
	break;*/

	  case SC_Halt:
		  DEBUG(‘a’, "\n Shutdown, initiated by user program.");
		  printf("\n\n Shutdown, initiated by user program.");
		  interrupt->Halt();
		  break;
	  case SC_Create:
	  {
		  int virtAddr;
		  char* filename;
		  DEBUG(‘a’, "\n SC_Create call ...");
		  DEBUG(‘a’, "\n Reading virtual address of filename");
		  // Lấy tham số tên tập tin từ thanh ghi r4 
		  virtAddr = machine->ReadRegister(4);
		  DEBUG(‘a’, "\n Reading filename.");
		  // MaxFileLength là = 32 
		  filename = User2System(virtAddr, MaxFileLength + 1);
		  if (filename == NULL)
		  {
			  printf("\n Not enough memory in system");
			  DEBUG(‘a’, "\n Not enough memory in system");
			  machine->WriteRegister(2, -1); // trả về lỗi cho chương          
			  // trình người dùng 
			  delete filename;
			  return;
		  }
		  DEBUG(‘a’, "\n Finish reading filename.");
		  DEBUG(‘a’,"\n File name : '"<<filename<<"'"); 
		  // Create file with size = 0 
		  // Dùng đối tượng fileSystem của lớp OpenFile để tạo file,  
		  // việc tạo file này là sử dụng các  thủ tục tạo file của hệ điều  
		  // hành Linux, chúng ta không quản ly trực tiếp các block trên 
		  // đĩa cứng cấp phát cho file, việc quản ly các block của file  
		  // trên ổ đĩa là một đồ án khác 
		  if (!fileSystem->Create(filename, 0))
		  {
			  printf("\n Error create file '%s'", filename);
			  machine->WriteRegister(2, -1);
			  delete filename;
			  return;
		  }
		  machine->WriteRegister(2, 0); // trả về cho chương trình  
		  // người dùng thành công 
		  delete filename;
		  break;
	  }
	  case SC_Read:
		  int virtAddr = machine->ReadRegister(4); // Get the virtual address of the buffer
		  int size = machine->ReadRegister(5);     // Get the size to read
		  OpenFileID id = machine->ReadRegister(6); // Get the file descriptor

		  if (size < 0)
		  {
			  machine->WriteRegister(2, -1);
			  return;
		  }
		  char* buffer = new char[size + 1];
		  if (id == CONSOLE_INPUT) {
			  // Read from console input (keyboard)
			  int bytesRead = SynchConsoleInput(buffer, size);
			  buffer[bytesRead] = '\0'; // Null-terminate the string
			  System2User(virtAddr, bytesRead, buffer);
			  machine->WriteRegister(2, bytesRead); // Set the result to the number of bytes read
		  }
		  else {
			  // Read from a file using the file descriptor (id)
			  int bytesRead = Read(buffer, size, id);
			  if (bytesRead == -1) {
				  machine->WriteRegister(2, -1); // Error
			  }
			  else {
				  buffer[bytesRead] = '\0'; // Null-terminate the string
				  System2User(virtAddr, bytesRead, buffer);
				  machine->WriteRegister(2, bytesRead); // Set the result to the number of bytes read
			  }
		  }
		  delete[] buffer;
		  break;
	  case SC_Write:
		  int writeVirtAddr = machine->ReadRegister(4); // Get the virtual address of the buffer to write
		  int writeSize = machine->ReadRegister(5);     // Get the size to write
		  OpenFileID writeId = machine->ReadRegister(6); // Get the file descriptor

		  char* writeBuffer = User2System(writeVirtAddr, writeSize);

		  if (writeId == CONSOLE_OUTPUT) {
			  // Write to console output (screen)
			  int bytesWritten = SynchConsoleOutput(writeBuffer, writeSize);
			  machine->WriteRegister(2, bytesWritten); // Set the result to the number of bytes written
			  return;
		  }
		  else {
			  // Write to a file using the file descriptor (writeId)
			  int bytesWritten = Write(writeBuffer, writeSize, writeId);
			  if (bytesWritten == -1) {
				  machine->WriteRegister(2, -1); // Error
			  }
			  else {
				  machine->WriteRegister(2, bytesWritten); // Set the result to the number of bytes written
			  }
		  }	
		  delete[] writeBuffer;
		  break;
	  case SC_Seek: {
		  int position = machine->ReadRegister(4); // Đọc vị trí từ thanh ghi r4
		  OpenFileID id = machine->ReadRegister(5); // Đọc mô tả tệp từ thanh ghi r5

		  if (id == CONSOLE_INPUT || id == CONSOLE_OUTPUT) {
			  // Báo lỗi không thể sử dụng Seek trên console
			  printf("\nKhong the seek tren file console.");
			  machine->WriteRegister(2, -1); // Trả về lỗi
		  }
		  else {
			  // Kiểm tra xem id có hợp lệ không
			  if (id < 2 || id > MAX_OPEN_FILES || fileSystem->openf(id)==NULL) {
				  // File không hợp lệ hoặc không mở
				  machine->WriteRegister(2, -1); // Trả về lỗi
			  }
			  else {
				  // Mở tệp tin với mô tả tệp id
				  if (position == -1) {
					  // Seek đến cuối tệp tin
					  position = fileSystem->openf[id];
				  }

				  if (position < 0 || position >fileSystem->openf[id]->Length()) {
					  // Vị trí không hợp lệ
					  machine->WriteRegister(2, -1); // Trả về lỗi
				  }
				  else {
					  fileSystem->openf[id]->Seek(position);
					  machine->WriteRegister(2, position); // Trả về vị trí mới
				  }
			  }
		  }
		  break;
	  }
	  case SC_Remove: {
		  int virtAddr = machine->ReadRegister(4); // Đọc địa chỉ ảo của tên tệp từ thanh ghi r4
		  char* name = User2System(virtAddr, MaxFileNameLength + 1); // Chuyển đổi địa chỉ ảo thành tên tệp

		  bool fileIsOpen = false;
		  // Kiểm tra xem tệp có đang mở không
		  for (int id = 2; id < MAX_OPEN_FILES; id++) {
			  if (fileTable->IsFileOpen(id) && strcmp(fileTable->GetFileName(id), name) == 0) {
				  // Tệp tin đang mở, không thể xóa
				  fileIsOpen = true;
				  break;
			  }
		  }
		  if (fileIsOpen) {
			  machine->WriteRegister(2, -1); // Trả về lỗi
		  }
		  else {
			  if (fileSystem->Remove(name)) {
				  // Xóa tệp tin thành công
				  machine->WriteRegister(2, 0);
			  }
			  else {
				  // Lỗi khi xóa tệp tin
				  machine->WriteRegister(2, -1);
			  }
		  }
		  delete[] name;
		  break;
	  }
	  default:
		  printf("\n Unexpected user mode exception (%d %d)", which,
			  type);
		  interrupt->Halt();
	  }
	}
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
