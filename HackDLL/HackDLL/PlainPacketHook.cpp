#include "stdafx.h"

#include "HackDLL.h"
#include "MemoryScanner.h"

#include "SendHook.h"
#include "RecvHook.h"
#include "ChatHook.h"

#include "Packets.h"
#include "PlainPacketHook.h"

long WINAPI HookingHandler(PEXCEPTION_POINTERS ExceptionInfo);

LPVOID lpEncryptFunctionAddr = NULL;
LPVOID lpDecryptFunctionAddr = NULL;
LPVOID lpMakePacketFunctionAddr = NULL;
LPVOID lpMakeChatFunctionAddr = NULL;

///////////////////////////////////////////////////////////////////////////////
// Hook functions using veh handler

int WINAPI HookPlainPacket() {
	// function encrypt packets (within WS2_32.send)
	std::vector<LPVOID> list;
	BYTE pattern[] = { 0x40, 0x53, 0x41, 0x54, 0x48, 0x83, 0xEC, 0x48, 0x4C, 0x8D, 0x49, 0x04 };
	MemoryScan(pattern, sizeof(pattern), list);

	lpEncryptFunctionAddr = (LPVOID)((SIZE_T)list.front() + 4);
	printf("[*] Encryption : %p\n", lpEncryptFunctionAddr);

	// function decrypt packets (within WSOCK32.recv)
	list.clear();
	BYTE pattern2[] = { 0x0f, 0x8d, 0x03, 0xff, 0xff, 0xff, 0x48, 0x8b, 0x5c, 0x24, 0x30, 0x48, 0x8b, 0x6c, 0x24, 0x38, 0x48, 0x8b, 0x74, 0x24, 0x40, 0x48, 0x83, 0xc4, 0x20 };
	MemoryScan(pattern2, sizeof(pattern2), list);

	lpDecryptFunctionAddr = (LPVOID)((SIZE_T)list.front() + 21);
	printf("[*] Decrypt : %p\n", lpDecryptFunctionAddr);

	// function generate packets
	list.clear();
	BYTE pattern3[] = { 0x48, 0x8B, 0x74, 0x24, 0x50, 0x48, 0x8B, 0xC5, 0x48, 0x8B, 0x6C, 0x24, 0x48, 0x48, 0x83, 0xC4, 0x30, 0x5F, 0xC3, 0xCC };
	MemoryScan(pattern3, sizeof(pattern3), list);

	lpMakePacketFunctionAddr = (LPVOID)((SIZE_T)list.at(3) + 5);
	printf("[*] Packet : %p\n", lpMakePacketFunctionAddr);

	// function generate chat data
	list.clear();
	BYTE pattern4[] = { 0xe8, 0x78, 0x7d, 0xd7, 0xff, 0x48, 0x8b, 0x5c, 0x24, 0x30, 0x48, 0x8b, 0x6c, 0x24, 0x38, 0x48, 0x8b, 0xc6, 0x48, 0x8b, 0x74, 0x24, 0x40, 0x48, 0x83, 0xc4, 0x20, 0x5f, 0xc3 };
	MemoryScan(pattern4, sizeof(pattern4), list);
	
	lpMakeChatFunctionAddr = (LPVOID)((SIZE_T)list.front() + 23);
	printf("[*] Chat : %p\n", lpMakeChatFunctionAddr);

	// (debug) save addresses in "DR_Registers.txt" 
	FILE *fp = fopen("DR_Registers.txt", "wt");
	fprintf(fp, "%p %p %p %p\n", lpEncryptFunctionAddr, lpDecryptFunctionAddr, lpMakeChatFunctionAddr, lpMakePacketFunctionAddr);
	fclose(fp);

	// Set VEH Handler and Main thread context
	if (AddVectoredExceptionHandler(1, (PVECTORED_EXCEPTION_HANDLER)HookingHandler) == NULL) {
		printf("[*] HookPlainText : Couldn't add vectored exception handler..\n");
		return 1;
	}

	CONTEXT ctx;
	memset(&ctx, 0, sizeof(CONTEXT));
	ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

	ctx.Dr0 = (DWORD64)lpEncryptFunctionAddr;
	ctx.Dr1 = (DWORD64)lpDecryptFunctionAddr;
	ctx.Dr2 = (DWORD64)lpMakePacketFunctionAddr;
	ctx.Dr3 = (DWORD64)lpMakeChatFunctionAddr;

	ctx.Dr7 |= (0x1 | 0x4 | 0x10 | 0x40 );

	HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, GetMainThreadId());

	SetThreadContext(hThread, &ctx);
	CloseHandle(hThread);

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// VEH Handler : Hook such functions through dr registers and veh handler

long WINAPI HookingHandler(PEXCEPTION_POINTERS ExceptionInfo) {
	PEXCEPTION_RECORD pExceptionRecord = ExceptionInfo->ExceptionRecord;
	PCONTEXT pContext = ExceptionInfo->ContextRecord;

	char * lpplaintext;
	char buf[1024] = { 0, };
	SIZE_T ptsize;
	FILE * fp;
	FILE * recvfp;
	int i = 0;
	int j = 0;
	
	if (pExceptionRecord->ExceptionAddress == lpEncryptFunctionAddr) {
		isPlainSendPacket = false;

		pContext->Rip = (DWORD64)lpEncryptFunctionAddr + 4;
		pContext->Rsp -= 0x48;

		lpplaintext = (char *)pContext->Rdx;
		ptsize = pContext->R8;
		
		if (isPosionIndexSet == FALSE && !memcmp(lpplaintext, "\x0C\x00\xC9\x56", 4)) { // Need Revised...
			
			memcpy((char *)&posionbuf[4], (char *)&lpplaintext[4], 2);


			printf("isPosionIndexSet = TRUE\n");
			isPosionIndexSet = TRUE;
		}
		
		else if (isMacroOn == TRUE && !memcmp(lpplaintext, "\x04\x00\x19\x52", 4)) {
			printf("[ Target Dead ]\n");
			isEliminated = TRUE;
		}

		PrintHexData((unsigned char *)pContext->Rdx, pContext->R8, true); //Rdx = PacketAddr, R8 = PacketLength

		return EXCEPTION_CONTINUE_EXECUTION;
	}
	if (pExceptionRecord->ExceptionAddress == lpDecryptFunctionAddr) {	// RECV PLAINTEXT
		isPlainRecvPacket = false;

		pContext->Rip = (DWORD64)lpDecryptFunctionAddr + 4;
		pContext->Rsp += 0x20;

		lpplaintext = (char *)pContext->Rbp;

		lpplaintext = (char *)pContext->Rbp;
		ptsize = pContext->Rax;

		if (isGettingLoc)
			ParseRecvData(lpplaintext, ptsize);
		else {
			if (!memcmp(lpplaintext, "\x2D\x00\x41\x51", 4))	// if Location Packet
				if (setTarget == FALSE && isMacroOn == TRUE && !(memcmp(lpplaintext + 8, "\x00\x80\x0B\x00", 4))) {

					printf("[ Set Target Location ]\n");
					PrintHexData((unsigned char *)lpplaintext, 0x2D, 0);

					// Build Teleport Packet
					memcpy(targetinfo.Index, &(lpplaintext[4]), 8);

					memcpy((char *)&playerattackinfo.X, &(lpplaintext[12]), 4);
					memcpy((char *)&playerattackinfo.Y, &(lpplaintext[16]), 4);
					memcpy((char *)&playerattackinfo.Z, &(lpplaintext[20]), 4);
					memcpy((char *)&playerattackinfo.Dir, &(lpplaintext[24]), 2);

					// Direction : if X is set, Direction would be zero...
					playerattackinfo.X -= 0.3;
					//playerattackinfo.Y -= 0.1;

					//memcpy(teleportbuf, "\x2E\x00\x0B\x52", 4);
					//memcpy(teleportbuf + 4, "\x41\xA6\x0F\x00", 4);
					memcpy(teleportbuf + 8, (char *)&playerattackinfo.X, 4);
					memcpy(teleportbuf + 12, (char *)&playerattackinfo.Y, 4);
					memcpy(teleportbuf + 16, (char *)&playerattackinfo.Z, 4);
					memcpy(teleportbuf + 20, (char *)&playerattackinfo.Dir, 2);
					memset(teleportbuf + 22, 0, 24);


					// Set lockonbuf
					memcpy(lockonbuf + 4, targetinfo.Index, 8);


					//recvfp = fopen("D:\\teleport.txt", "w+");	// teleport.txt : if this packet is sended, character would teleport to target.
					
					//fprintf(recvfp, "1\n");
					
					//for (i = 0; i < 46; i++)
					//	fprintf(recvfp, "%02X ", *((unsigned char *)&buf[i]));
					
					//fclose(recvfp);

					setTarget = TRUE;
				}

				if (!(ptsize == 45))
					ParseRecvData(lpplaintext, ptsize);
		}

		PrintHexData((unsigned char *)pContext->Rbp, pContext->Rax, false); //Rbp = PacketAddr, Rax = PacketLength

		return EXCEPTION_CONTINUE_EXECUTION;
	}
	else if (pExceptionRecord->ExceptionAddress == lpMakePacketFunctionAddr) {
		pContext->Rip = (DWORD64)lpMakePacketFunctionAddr + 3;
		pContext->Rax = pContext->Rbp;

		//extern variable "isHooked" in Packets.h
		if (isHooked) {
			PacketReplacer(*(unsigned char **)pContext->Rbp, (int *)&pContext->R8);
		}
		
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	else if (pExceptionRecord->ExceptionAddress == lpMakeChatFunctionAddr) {
		pContext->Rip = (DWORD64)lpMakeChatFunctionAddr + 4;
		pContext->Rsp += 0x20;
	
		ChatHook((unsigned char *)pContext->Rdx);
	
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	else {
		return EXCEPTION_CONTINUE_SEARCH;
	}
}
