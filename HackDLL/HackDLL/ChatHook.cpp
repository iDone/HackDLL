#include "stdafx.h"
#include "HackDLL.h"
#include "Packets.h"
#include "ChatHook.h"

#define CHAT_DATA_INDEX 54

char filename[1024] = { 0, };

///////////////////////////////////////////////////////////////////////////////
// Routine from generating chat function

int ChatHook(unsigned char *packet) {
	WCHAR* ptr = wcschr((WCHAR *)(packet + CHAT_DATA_INDEX), '<');
	*ptr = NULL;

	// stop macro
	// extern variable "isPlaying" in Packets.h
	if (isMacroOn) {
		if (wcschr((WCHAR *)(packet + CHAT_DATA_INDEX), 'm')) {
			isMacroOn = false;
			*ptr = '<';
		}
		else {
			int tmp = 0;
			UnitPacketReplacer(packet, &tmp);
			//PacketReplacer(packet, &tmp);
		}
	}
	// start macro
	else if (wcsstr((WCHAR *)(packet + CHAT_DATA_INDEX), L"macro ")) {
		WCHAR *ptrb = wcschr((WCHAR *)(packet + CHAT_DATA_INDEX), ' ');
		sprintf(filename, "%ls", ptrb + 1);
		
		if (!access(filename, 0)) {
			printf("[*] Macro starts..\n");
		
			isMacroOn = TRUE;
			CreateRemoteThread(GetCurrentProcess(), NULL, 0, (LPTHREAD_START_ROUTINE)PlayMacro, NULL, 0, NULL);
		}

		*ptr = '<';
	}
	// replace packet
	// extern variable "isHooked" in Packets.h
	else if (wcsstr((WCHAR *)(packet + CHAT_DATA_INDEX), L"packets ")) {
		WCHAR *ptrb = wcschr((WCHAR *)(packet + CHAT_DATA_INDEX), ' ');
		sprintf(filename, "%ls", ptrb + 1);

		if (!access(filename, 0)) {
			printf("[*] replacing packets : %s\n", filename);
			isHooked = true;
		}

		*ptr = '<';
	}
	// modify chat with replacing such entity code
	else if (wcsstr((WCHAR *)(packet + CHAT_DATA_INDEX), L"modify_chat ")) {
		WCHAR *ptrb = wcschr((WCHAR *)(packet + CHAT_DATA_INDEX), ' ');
		SIZE_T origlen = wcslen((WCHAR *)(packet + CHAT_DATA_INDEX));
		SIZE_T newlen = wcslen(ptrb + 1);

		printf("[*] chat modifying : %ls\n", ptrb + 1);

		unsigned char chat[1024] = { 0, };
		memcpy(chat, packet, CHAT_DATA_INDEX);
		memcpy(chat + CHAT_DATA_INDEX, ptrb + 1, newlen * 2);

		newlen = EntityReplacer((WCHAR *)(chat + CHAT_DATA_INDEX), L"&lt;", '<', newlen);
		newlen = EntityReplacer((WCHAR *)(chat + CHAT_DATA_INDEX), L"&gt;", '>', newlen);
		newlen = EntityReplacer((WCHAR *)(chat + CHAT_DATA_INDEX), L"&quot;", '\"', newlen);

		*ptr = '<';
		memcpy(chat + CHAT_DATA_INDEX + newlen * 2, ptr, *packet - CHAT_DATA_INDEX - origlen * 2);
		*chat = *packet - origlen * 2 + newlen * 2;

		memcpy(packet, chat, *chat);
	}
	else if (wcsstr((WCHAR *)(packet + CHAT_DATA_INDEX), L"stop loc info")){	//Stop Capturing Monster Location Info
		isGettingLoc = FALSE;
	}
	else if (wcsstr((WCHAR *)(packet + CHAT_DATA_INDEX), L"start loc info")) {
		isGettingLoc = TRUE;

	}
	else {
		*ptr = '<';
	}

	return 0;
}