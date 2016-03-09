#pragma once

#include "CPacketSender.h"
#include "CPacketReceiver.h"
#include "C2DGraphics.h"
#include "Dialogs.h"

using namespace std;

#define CHECK_NEW_DEVICES_INTERVAL_MS 8000    //length of time in milliseconds after switching to full-screen when we should check for addition of new devices

//User preferences that are persisted
typedef struct PrefStruct
{
	char LoginID[32];
	char Password[32];
	char Callsign[32];
	char ICAOType[32];
	long PTTVKey;
	long PTTJoyButton;
} PrefStruct;

//Description of a D3D device
typedef struct DeviceDescStruct
{
	IDirect3DDevice9 *pDevice;
	HWND hWnd;

} DeviceDescStruct;


typedef class CFSXGUI
{
public:
	CFSXGUI();
	~CFSXGUI();

	//////////////////
	//Called by GUI-FSX functions from FSX
	void Initialize();
	void Shutdown();
	void OnFSXPresent(IDirect3DDevice9 *pI);
	void OnFSXSimRunning();
	void OnFSXSimStopped();
	void OnFSXAddonMenuSelected();

	LRESULT ProcessFSXWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	//////////////////
	//Called by dialogs

	//Add dialog to the open dialog list, at top-most spot
	void AddDialog(CDialog *pDialog);

	//Remove dialog from the open dialog list
	void RemoveDialog(CDialog *pDialog);

	//Move given already-added dialog to the top-most spot 
	void SetTopmostDialog(CDialog *pDialog);

	//Indicate some dialog needs mouse move messages (true) or no longer (false)
	void IndicateNeedMouseMove(bool bNeedMouseMove);

	//Indicate some dialog needs keyboard keys (true) or no longer (false)
	void IndicateNeedKeyboard(bool bNeedKeyboard);

	C2DGraphics m_Graphics;

protected:

	HWND        m_hFSXWindow;
	bool		m_bGraphicsInitialized; 
	bool		m_bRunning;						//True if we're running  
	bool        m_bNeedMouseMove;				//True if we should forward mouse move messages to the dialogs (e.g. dragging some dialog)
	bool		m_bNeedKeyboard;				//True if we should forward keystrokes to the dialogs (e.g. edit box is active)
	PrefStruct  m_Prefs;						//User preferences
	LONG_PTR	m_FSXWindowProc;				//FSX's main windows procedure
	
	//List of devices and current display mode
	vector<DeviceDescStruct> m_aFullscreenDevices; //D3D fullscreen devices 
	DeviceDescStruct m_WindowedDeviceDesc;		//D3D windowed device
	IDirect3DDevice9 *m_pFullscreenPrimaryDevice; //Main device (monitor) in fullscreen mode

	bool		m_bInWindowedMode;				//True if we are in windowed mode
	bool	    m_bCheckForNewDevices;			//True if we should monitor for new devices (e.g. after switching from windows to fullscreen)
	DWORD		m_dwCheckNewDevicesEndTime;		//GetTickCount() time after which we should stop checking

	//Dialogs
	vector<CDialog *> m_apDialogs;				//Pointers to all the base classes below for easy iteration
	vector<CDialog *> m_apOpenDialogs;          //List of which ones are open (0 is bottom-most in draw order)

	CMainDlg	m_dlgMain;

	// Internal methods

	void InitGraphics(IDirect3DDevice9 *pI);
	void CheckIfNewDevice(IDirect3DDevice9 *pI);
	void OnPTTButton(bool bPressed);
	void NotifyDialogsNowWindowed(HWND hWnd);
	void NotifyDialogsNowFullscreen(IDirect3DDevice9 *pI, int BackBufferWidth, int BackBufferHeight);

} CFSXGUI;

