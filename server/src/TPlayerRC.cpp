#include <vector>
#include <math.h>
#include "ICommon.h"
#include "TPlayer.h"
#include "TAccount.h"
#include "CSocket.h"
#include "TServerList.h"

#define serverlog	server->getServerLog()
#define rclog		server->getRCLog()

bool TPlayer::msgPLI_RC_CHAT(CString& pPacket)
{
	CString chatMsg = pPacket.readString("");
	if ( chatMsg[0] != '/' )
	{
		server->sendPacketTo(CLIENTTYPE_RC, CString() >> (char)PLO_RCMESSAGE << nickName << ": " << chatMsg);
	}
	else
	{
		std::vector<CString> tokens = chatMsg.tokenize();
		if (tokens[0] == "/reconnect")
		{
			sendPacket(CString() >> (char)PLO_RCMESSAGE << "Reconnecting to the NPC-Server." );
			sendPacket(CString() >> (char)PLO_NPCSERVERADDR >> (char)0 >> (char)2 << server->getNPCServerAddr());
			sendPacket(CString() >> (char)PLO_RCMESSAGE << "Reconnecting to the NPC-Server." );
		}
	}
	return true;
}