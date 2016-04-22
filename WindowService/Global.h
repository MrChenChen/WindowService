#pragma once
#include <windows.h>
#include <iostream>
#include <Wtsapi32.h>
#include <Userenv.h>
#include "ServiceBase.h"
#include "ServiceInstaller.h"

#include <TlHelp32.h>


#include <iostream>
#include <string>
#include <strstream>
#include <fstream>

#include <process.h>

using namespace std;

#pragma comment(lib,"Wtsapi32.lib")
#pragma comment(lib,"Userenv.lib")


string Char2WChar(const wchar_t* wide)
{
	wstring wstrValue(wide);

	string strValue;
	strValue.assign(wstrValue.begin(), wstrValue.end());  // convert wstring to string

	return strValue;
}


LPWSTR IntToLPWSTR(long long n)
{
	strstream ss;
	string s;
	ss << n;
	ss >> s;

	int dwLen = strlen(s.c_str()) + 1;
	int nwLen = MultiByteToWideChar(CP_ACP, 0, s.c_str(), dwLen, NULL, 0);//算出合适的长度
	LPWSTR lpszPath = new WCHAR[dwLen];
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), dwLen, lpszPath, nwLen);
	return lpszPath;
}


void ShowMessage(LPWSTR lpszMessage, LPWSTR lpszTitle = L"From Service")
{

	DWORD dwSession = WTSGetActiveConsoleSessionId();

	DWORD dwResponse = 0;

	WTSSendMessage(WTS_CURRENT_SERVER_HANDLE, dwSession,

		lpszTitle,

		static_cast<DWORD>((wcslen(lpszTitle) + 1) * sizeof(wchar_t)),

		lpszMessage,

		static_cast<DWORD>((wcslen(lpszMessage) + 1) * sizeof(wchar_t)),

		0, 0, &dwResponse, FALSE);

}




DWORD FindSessionPid(LPSTR lpProcessName, DWORD dwSessionId)
{
	DWORD res = 0;

	PROCESSENTRY32 procEntry;

	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == INVALID_HANDLE_VALUE)
	{
		return res;
	}

	procEntry.dwSize = sizeof(PROCESSENTRY32);

	if (!Process32First(hSnap, &procEntry))
	{
		goto _end;
	}

	do
	{
		if (Char2WChar(procEntry.szExeFile) == string(lpProcessName))
		{
			DWORD winlogonSessId = 0;
			if (ProcessIdToSessionId(procEntry.th32ProcessID, &winlogonSessId) && winlogonSessId == dwSessionId)
			{
				res = procEntry.th32ProcessID;
				break;
			}
		}

	} while (Process32Next(hSnap, &procEntry));

_end:
	CloseHandle(hSnap);
	return res;
}


void RaiseAssess(HANDLE & hToken)
{

	LPVOID TokenInformation;

	DWORD RetLen = 0;

	TOKEN_PRIVILEGES tp;

	LUID luid;

	DWORD UserHwnd = 0;

	while (!(UserHwnd = FindSessionPid("explorer.exe", WTSGetActiveConsoleSessionId())))
	{
		Sleep(1000);
	};

	if (!OpenProcessToken(OpenProcess(PROCESS_ALL_ACCESS, FALSE, UserHwnd), TOKEN_ALL_ACCESS_P, &hToken))
	{
		ShowMessage(IntToLPWSTR(GetLastError()), L"OpenProcessToken");
	}

	if (!GetTokenInformation(hToken, TokenLinkedToken, &TokenInformation, 4, &RetLen))
	{
		ShowMessage(IntToLPWSTR(GetLastError()), L"GetTokenInformation");
	}
	else
	{
		hToken = TokenInformation;
	}

	if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid))
	{
		ShowMessage(IntToLPWSTR(GetLastError()), L"LookupPrivilegeValue");
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_USED_FOR_ACCESS;

	if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL))
	{
		ShowMessage(IntToLPWSTR(GetLastError()), L"AdjustTokenPrivileges");
	}


}


void StartProcess(PWSTR _startexe)
{
	BOOL bSuccess = FALSE;

	STARTUPINFO si = { 0 };

	PROCESS_INFORMATION pi = { 0 };

	si.cb = sizeof(si);

	HANDLE hToken = NULL;

	HANDLE hDuplicatedToken = NULL;

	DWORD dwSessionID = WTSGetActiveConsoleSessionId();


	RaiseAssess(hToken);


	if (!DuplicateTokenEx(hToken, MAXIMUM_ALLOWED, NULL, SecurityIdentification, TokenPrimary, &hDuplicatedToken))
	{
		ShowMessage(IntToLPWSTR(GetLastError()), L"DuplicateTokenEx");
	}

	LPVOID lpEnvironment = NULL;

	if (!CreateEnvironmentBlock(&lpEnvironment, hDuplicatedToken, FALSE))
	{
		ShowMessage(IntToLPWSTR(GetLastError()), L"CreateEnvironmentBlock");
	}

	if (!CreateProcessAsUser(hDuplicatedToken, _startexe, NULL, NULL, NULL, FALSE,
		NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT,
		lpEnvironment, NULL, &si, &pi))
	{
		ShowMessage(IntToLPWSTR(GetLastError()), L"CreateProcessAsUser");
	}

	//Clean

	CloseHandle(pi.hProcess);

	CloseHandle(pi.hThread);

	bSuccess = TRUE;

	if (hToken != NULL) CloseHandle(hToken);

	if (hDuplicatedToken != NULL) CloseHandle(hDuplicatedToken);

	if (lpEnvironment != NULL) DestroyEnvironmentBlock(lpEnvironment);

}


static void ServiceWorkerThread(void* arg)
{
	PWSTR * startexe = static_cast<PWSTR*>(arg);

	if (*startexe != L"")
	{

		fstream _file;
		_file.open(*startexe, ios::in);

		if (_file) StartProcess(*startexe);
		_file.close();
	}
}

class MyService :public CServiceBase
{
public:

	MyService() = delete;

	MyService(PWSTR pszServiceName, PWSTR _startexe = L"",
		BOOL fCanStop = TRUE,
		BOOL fCanShutdown = TRUE,
		BOOL fCanPauseContinue = FALSE) :CServiceBase(pszServiceName, fCanStop, fCanShutdown, fCanPauseContinue)
	{
		m_fStopping = FALSE;

		startexe = _startexe;

		// Create a manual-reset event that is not signaled at first to indicate 
		// the stopped signal of the service.

		m_hStoppedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		if (m_hStoppedEvent == NULL)
		{
			throw GetLastError();
		}

	}



	virtual ~MyService(void) = default;

protected:

	virtual void OnStart(DWORD dwArgc, PWSTR *pszArgv)
	{

		_beginthread(ServiceWorkerThread, 0, &startexe);

	}



	virtual void OnStop()
	{

	}
private:

	PWSTR startexe;

	BOOL m_fStopping;
	HANDLE m_hStoppedEvent;
};

