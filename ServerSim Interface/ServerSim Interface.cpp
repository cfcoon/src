//This EXE simulates the Vatsim server interface,
//to create test data for development. 

#include "stdafx.h"

#include "Packets.h"
#include "CPacketSender.h"
#include "CPacketReceiver.h"
#include "CTestAircraft.h"

//Globals
CPacketSender g_Sender;
CPacketReceiver g_Receiver;
bool g_bQuit = false;
bool g_bUserSentFirstUpdate = false; //flag so when we receive first user update, we spawn all aircraft
bool g_bUserConnected = false;
#define NUM_AIRCRAFT 12  
CTestAircraft g_Aircraft[NUM_AIRCRAFT];

//Forward declarations
void ProcessPacket(void *pPacket);
void SpawnAllAircraft(UserStateUpdatePacket *p);

int _tmain(int argc, _TCHAR* argv[])
{
	printf("Server Simulator\n");

	//Initialize the packet sender and receiver
	g_Sender.Initialize(CLIENT_LISTEN_PORT);
	g_Receiver.Initialize(SERVER_INTERFACE_LISTEN_PORT);

	while (1)     //DEBUG so we can leave this EXE running while developing. Once we're integrated with FSX where FSX launches 
 			     //  the client which launches us, we can remove this. 
	{
		printf("\nRestarted\n");
		g_bQuit = false;
		g_bUserSentFirstUpdate = false;
		g_bUserConnected = false;

		printf("Waiting for messages...\n");

		//Main loop
		char PacketBuffer[LARGEST_PACKET_SIZE];
		while (!g_bQuit)
		{
			//Process any incoming packets
			while (g_Receiver.GetNextPacket(&PacketBuffer))
				ProcessPacket(&PacketBuffer);

			//If user has sent their initial position, it means our aircraft are spawned and sim is running
			if (g_bUserSentFirstUpdate)
			{
				for (int i = 0; i < NUM_AIRCRAFT; i++)
					g_Aircraft[i].Update();
				Sleep(1);
			}
			//Otherwise we're just waiting for connection... no hurry
			else
				Sleep(500);
		}

		for (int i = 0; i < NUM_AIRCRAFT; i++)
			g_Aircraft[i].Stop();

	}
 
	g_Sender.Shutdown();
	g_Receiver.Shutdown();


	ExitProcess(0);
	
}

void ProcessPacket(void *pPacket)
{
	static ConnectSuccessPacket P;
	static ReqUserStatePacket R;

	switch (((PacketHeader *)pPacket)->Type)
	{
	case REQ_CONNECTION_PACKET:
		//Send connect success
		if (!g_bUserConnected)
		{
			strcpy_s(P.szMessage, 128, "Welcome to the test server.");
			g_Sender.Send(&P); 
			printf("REQ CONNECTION received; sent CONNECT_SUCCESS and REQ_USER_STATE\n");

			//Send request user state. We will spawn the test aircraft around it when we receive it		
			g_Sender.Send(&R);
			g_bUserConnected = true;
		}
		break;

	case USER_STATE_UPDATE_PACKET:

		printf("Received user state\n");
		
		if (!g_bUserSentFirstUpdate)
		{
			SpawnAllAircraft((UserStateUpdatePacket *)pPacket);
			g_bUserSentFirstUpdate = true;
		}
		break;

	case TRANSMIT_KEYDOWN_PACKET:
		break;

	case TRANSMIT_KEYUP_PACKET:
		break;

	case REQ_DISCONNECT_PACKET:
		//Tell client success
		printf("Received REQ_DISCONNECT, sending LOGOFF_SUCCESS and restarting server.\n");
		LogoffSuccessPacket LSPack;
		g_Sender.Send(&LSPack);
		g_bUserConnected = false;

		//Flag to exit
		g_bQuit = true;
		break;

	}
	return;
}

void SpawnAllAircraft(UserStateUpdatePacket *p)
{
	g_Aircraft[0].Initialize("TUP_0_TRUTH", "B738", p->LatDegN, p->LonDegE, p->HdgDegTrue, p->AltFtMSL, TAXI, NO_LAG, 0.03, &g_Sender, 0);
	g_Aircraft[1].Initialize("TUP_1_TYPLAG", "B738", p->LatDegN, p->LonDegE, p->HdgDegTrue, p->AltFtMSL, TAXI, TYPICAL_LAG, 1.0, &g_Sender, 1);
	g_Aircraft[2].Initialize("TUP_1_HILAG", "B738", p->LatDegN, p->LonDegE, p->HdgDegTrue, p->AltFtMSL, TAXI, HIGH_LAG, 1.0, &g_Sender, 2);
	g_Aircraft[3].Initialize("TUP_5_HILAG", "B738", p->LatDegN, p->LonDegE, p->HdgDegTrue, p->AltFtMSL, TAXI, HIGH_LAG, 5.0, &g_Sender, 3);
	g_Aircraft[4].Initialize("UP_0_TRUTH", "B738", p->LatDegN, p->LonDegE, p->HdgDegTrue, p->AltFtMSL, TOUCH_AND_GO, NO_LAG, 0.03, &g_Sender, 0);
	g_Aircraft[5].Initialize("UP_1_TYPLAG", "B738", p->LatDegN, p->LonDegE, p->HdgDegTrue, p->AltFtMSL, TOUCH_AND_GO, TYPICAL_LAG, 1.0, &g_Sender, 1);
	g_Aircraft[6].Initialize("UP_5_HILAG", "B738", p->LatDegN, p->LonDegE, p->HdgDegTrue, p->AltFtMSL, TOUCH_AND_GO, HIGH_LAG, 1.0, &g_Sender, 2);

	return;
}
	