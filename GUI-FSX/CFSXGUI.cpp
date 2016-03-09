#include "stdafx.h"
#include "CFSXGUI.h"

extern HMODULE g_hModule;
extern HWND g_hWnd; 
extern void SetAddonMenuText(char *Text);

#define MENU_TEXT "VatConnect"
const DWORD KEY_REPEAT = 1 << 30;
const DWORD ALT_PRESSED = 1 << 29;

/////////////////
//Hooked windows procedure
CFSXGUI *g_pGUI = NULL;
LRESULT CALLBACK FSXWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	return g_pGUI->ProcessFSXWindowMessage(hWnd, message, wParam, lParam);
}

//////////////////
//CFSXGUI

CFSXGUI::CFSXGUI() : m_bRunning(false), m_bGraphicsInitialized(false), m_FSXWindowProc(NULL), 
	m_bNeedMouseMove(false), m_bNeedKeyboard(false), m_bCheckForNewDevices(false),
	m_bInWindowedMode(true), m_pFullscreenPrimaryDevice(NULL)
{
	g_pGUI = this;
	m_WindowedDeviceDesc.hWnd = NULL;
	m_WindowedDeviceDesc.pDevice = NULL;
}

CFSXGUI::~CFSXGUI()
{
}

void CFSXGUI::Initialize()
{
	SetAddonMenuText(MENU_TEXT);

	//Load preferences
	//TODO
	strcpy_s(m_Prefs.LoginID, 32, "9999999");
	strcpy_s(m_Prefs.Password, 32, "123456");
	strcpy_s(m_Prefs.Callsign, 32, "DAL724");
	strcpy_s(m_Prefs.ICAOType, 32, "B738/Q");
	m_Prefs.PTTVKey = VK_CAPITAL;
	m_Prefs.PTTJoyButton = 1;

	return;
}

//Can be called externally (i.e. user closed FSX) or internally (user chose to disconnect)
void CFSXGUI::Shutdown()
{
	//DisconnectFromVATSIM();

	for (size_t i = 0; i < m_apDialogs.size(); i++)
	{
		m_apDialogs[i]->Shutdown();
	}
	
	m_Graphics.Shutdown();

	//Restore FSX Windows procedure chain back to original
	if (m_FSXWindowProc)
	{
		SetWindowLongPtr(m_hFSXWindow, GWLP_WNDPROC, m_FSXWindowProc);
		m_FSXWindowProc = NULL;
	}

	m_bRunning = false;

	return;
}

//FSX has finished drawing to given device... add any overlays on that device 
void CFSXGUI::OnFSXPresent(IDirect3DDevice9 *pI)
{
	
	if (!m_bRunning)
		return;
		
	if (!m_bGraphicsInitialized)
		InitGraphics(pI);

	//See if we've switched from windowed to full-screen, or back !REVISIT we should not have
	//to check for this every single frame, although there doesn't seem to be a better
	//way to do it (maybe trap alt-ENTER?). This does limit further polling to a short time
	//interval after we get a non-cached device though.  
	if (!m_bCheckForNewDevices)
	{
		if (m_bInWindowedMode)
		{
			if (pI != m_WindowedDeviceDesc.pDevice)
			{
				m_bCheckForNewDevices = true;
				m_dwCheckNewDevicesEndTime = GetTickCount() + CHECK_NEW_DEVICES_INTERVAL_MS;
			}
		}
		//Fullscreen mode, check if one of the known fullscreen devices 
		else
		{
			bool bFound = false;
			for (size_t i = 0; !bFound && i < m_aFullscreenDevices.size(); i++)
				if (m_aFullscreenDevices[i].pDevice == pI)
					bFound = true;
			if (!bFound)
			{
				m_bCheckForNewDevices = true;
				m_dwCheckNewDevicesEndTime = GetTickCount() + CHECK_NEW_DEVICES_INTERVAL_MS;
			}
		}
	}

	//Continue scan for new devices
	if (m_bCheckForNewDevices)
	{
		if (GetTickCount() < m_dwCheckNewDevicesEndTime)
			CheckIfNewDevice(pI);
		else
			m_bCheckForNewDevices = false;
	}

	//If windowed, or fullscreen primary device, tell each open dialog to draw
	if ((m_bInWindowedMode && pI == m_WindowedDeviceDesc.pDevice) ||
		(!m_bInWindowedMode && pI == m_pFullscreenPrimaryDevice))
	{
		for (size_t i = 0; i < m_apOpenDialogs.size(); i++)
			m_apDialogs[i]->Draw(pI);
	}

	return;
}

//FSX has gone into flight mode
void CFSXGUI::OnFSXSimRunning()
{
	return;
}

//FSX has gone out of flight mode (e.g. to options, or exited flight)
void CFSXGUI::OnFSXSimStopped()
{
	return;
}

//User has selected FSX addon menu ("VatConnect...")
void CFSXGUI::OnFSXAddonMenuSelected()
{
	if (m_bRunning)
		return;

	m_bRunning = true;
	m_dlgMain.Open();
	m_apDialogs.push_back(&m_dlgMain);
	m_apOpenDialogs.push_back(&m_dlgMain);

	return;
}

//Handle FSX's windows messages before FSX does (to intercept mouse and keyboard). Return code
//depends on the message.
LRESULT CFSXGUI::ProcessFSXWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	bool bHandled = false;
	int RetCode = 0;

	if (message < WM_USER)
	{
		if (message == WM_KEYDOWN || message == WM_KEYUP)
		{
			//Check if it's keyboard Push-to-talk (other keypresses we handle in WM_CHAR)
			if (m_Prefs.PTTVKey && (long)wParam == m_Prefs.PTTVKey)
			{
				//Only process first KEYDOWN	
				if (message == WM_KEYDOWN && !(lParam & KEY_REPEAT))
					OnPTTButton(true);
				else if (message == WM_KEYUP)
					OnPTTButton(false);
				bHandled = true;
			} 

			//If we have keyboard focus, return that this was handled and we'll process it in WM_CHAR
			else if (m_bNeedKeyboard)
				return 0;
		}

		//Forward to dialogs, except keyboard key if we aren't capturing it -- note they return our WINMSG enum
		if (message != WM_CHAR || m_bNeedKeyboard)
		{
			for (size_t i = 0; i < m_apOpenDialogs.size() && !bHandled; i++)
			{
				RetCode = m_apOpenDialogs[i]->WindowsMessage(message, wParam, lParam);
				if (RetCode != WINMSG_NOT_HANDLED)
					bHandled = true;
			}
		} 
	}
	if (!bHandled)
		return CallWindowProc((WNDPROC)m_FSXWindowProc, hWnd, message, wParam, lParam);

	//Certain messages have special return codes to indicate handled
	if (message == WM_SETCURSOR)
	{
		SetWindowLongPtr((HWND)wParam, DWLP_MSGRESULT, TRUE);
		return TRUE;
	}
	else if (message == WM_MOUSEACTIVATE)
	{
		SetWindowLongPtr((HWND)wParam, DWLP_MSGRESULT, MA_NOACTIVATEANDEAT);
		return MA_NOACTIVATEANDEAT;
	}

	//Other messages 0 means handled
	SetWindowLongPtr((HWND)wParam, DWLP_MSGRESULT, 0);
	return 0;   
}


///////////////////
//Internal

//Initialize graphics, and put up first dialog. This occurs after user enables VatConnect, i.e. FSX is
//running although we don't know yet if it's windowed or fullscreen. 
void CFSXGUI::InitGraphics(IDirect3DDevice9 *pI)
{
	assert(pI != NULL);
	if (!pI)
		return;

	//Initialize graphics library 
	m_Graphics.Initialize(pI); 

	//Hook into FSX's window procedure -- (FindWindow must come before CheckIfNewDevice so we know which
	//is the primary FSX window)
	HWND hTop = FindWindow(L"FS98MAIN", NULL);
	m_hFSXWindow = hTop;
	m_FSXWindowProc = SetWindowLongPtr(m_hFSXWindow, GWLP_WNDPROC, (LONG_PTR)FSXWndProc);

	//Determine if this is windowed device or one of the fullscreen ones and cache it
	CheckIfNewDevice(pI);

	//Initialize dialogs
	m_dlgMain.Initialize(this, &m_Graphics, m_hFSXWindow);

	m_bGraphicsInitialized = true; 
	return;
}

//Given device to draw to, see if we already have it cached and if not, cache it.
//Also check if we have switched mode (i.e. we were windowed and this device is 
//a fullscreen device, or we were full-screen and this is a windowed one).
void CFSXGUI::CheckIfNewDevice(IDirect3DDevice9 *pI)
{
	if (!pI)
	{
		assert(0);
		return;
	}
	//Check if already have it
	bool bAlreadyHave = false;
	if (m_WindowedDeviceDesc.pDevice == pI)
		bAlreadyHave = true;
	else for (size_t i = 0; !bAlreadyHave && i < m_aFullscreenDevices.size(); i++)
	{
		if (m_aFullscreenDevices[i].pDevice == pI)
			bAlreadyHave = true;
	}

	//If we don't have it, determine if it's windowed or full-screen device and cache it
	if (!bAlreadyHave)
	{
		IDirect3DSwapChain9 *pISwapChain;
		if (FAILED(pI->GetSwapChain(0, &pISwapChain)))
		{
			assert(0);
			return;
		}
		D3DPRESENT_PARAMETERS PP;
		if (FAILED(pISwapChain->GetPresentParameters(&PP)))
		{
			assert(0);
			pISwapChain->Release();
			return;
		}
		pISwapChain->Release();

		//Windowed? 
		if (PP.Windowed)
		{
			if (!m_bInWindowedMode || m_WindowedDeviceDesc.hWnd == NULL)
			{
				m_bInWindowedMode = true;
				m_pFullscreenPrimaryDevice = NULL;
				NotifyDialogsNowWindowed(PP.hDeviceWindow);
			}
			m_WindowedDeviceDesc.hWnd = PP.hDeviceWindow;
			m_WindowedDeviceDesc.pDevice = pI;
		}
		//Fullscreen -- add to list
		else
		{
			static DeviceDescStruct D;
			D.hWnd = PP.hDeviceWindow;
			D.pDevice = pI;
			m_aFullscreenDevices.push_back(D);
			
			//Make first fullscreen device the primary
			if (!m_pFullscreenPrimaryDevice)
			{
				m_pFullscreenPrimaryDevice = pI;
				m_bInWindowedMode = false;
				m_WindowedDeviceDesc.pDevice = NULL; //FSX deletes old windows device when switching to fullscreen
				NotifyDialogsNowFullscreen(pI, PP.BackBufferWidth, PP.BackBufferHeight);
			}
		}
	}

	return;
}
//Notify all dialogs we have switched to windowed mode (m_WindowedDeviceDesc)
void CFSXGUI::NotifyDialogsNowWindowed(HWND hWnd)
{
	for (size_t i = 0; i < m_apDialogs.size(); i++)
		m_apDialogs[i]->SwitchToWindowed(hWnd);
	return;

}

//Notify all dialogs we have switched to fullscreen mode, primary device = m_pFullscreenPrimaryDevice
void CFSXGUI::NotifyDialogsNowFullscreen(IDirect3DDevice9 *pI, int Width, int Height)
{
	for (size_t i = 0; i < m_apDialogs.size(); i++)
		m_apDialogs[i]->SwitchToFullscreen(pI, Width, Height);
	return;
}
 

//Push-to-talk button has been pressed (bPressed = true), or released (false)
void CFSXGUI::OnPTTButton(bool bPressed)
{

	return;
}


/////////////////////////////////////////////////////////////////////
//Called by dialogs

//Add dialog to the open dialog list, at top-most spot
void CFSXGUI::AddDialog(CDialog *pDialog)
{


	return;
}

//Remove dialog from the open dialog list
void CFSXGUI::RemoveDialog(CDialog *pDialog)
{

	return;
}

//Move given already-added dialog to the top-most spot 
void CFSXGUI::SetTopmostDialog(CDialog *pDialog)
{


	return;
}

//Indicate some dialog needs mouse move messages (true) or no longer (false)
void CFSXGUI::IndicateNeedMouseMove(bool bNeedMouseMove)
{
	m_bNeedMouseMove = bNeedMouseMove;

	return;
}

//Indicate some dialog needs keyboard keys (true) or no longer (false)
void CFSXGUI::IndicateNeedKeyboard(bool bNeedKeyboard)
{

	m_bNeedKeyboard = bNeedKeyboard;

	return;
}
