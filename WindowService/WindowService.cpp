// WindowService.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "Global.h"
#include "ServiceBase.h"
#include <iostream>

#include <windows.h>


using namespace std;

// Internal name of the service
#define SERVICE_NAME             L"MyService"

#define SERVICE_EXE              L"C:\\Program Files (x86)\\MindVision\\MVDCP.exe"

// Displayed name of the service
#define SERVICE_DISPLAY_NAME     L"MyService Sample Service"

// Service start options.
#define SERVICE_START_TYPE       SERVICE_AUTO_START

// List of service dependencies - "dep1\0dep2\0\0"
#define SERVICE_DEPENDENCIES     L""

// The name of the account under which the service should run
#define SERVICE_ACCOUNT          L""  //NT AUTHORITY\\LocalService

// The password to the service account name
#define SERVICE_PASSWORD         NULL

#define Install 0
#define UnInstall 1
#define ServiceRun 0


int wmain(int argc, wchar_t *argv[])
{

#if Install

	InstallService(
		SERVICE_NAME,               // Name of service
		SERVICE_DISPLAY_NAME,       // Name to display
		SERVICE_START_TYPE,         // Service start type
		SERVICE_DEPENDENCIES,       // Dependencies
		SERVICE_ACCOUNT,            // Service running account
		SERVICE_PASSWORD            // Password of the account
		);

#endif

#if UnInstall

	UninstallService(SERVICE_NAME);

#endif


#if ServiceRun

	MyService service(SERVICE_NAME, SERVICE_EXE);

	MyService::Run(service);

#endif

	system("Pause");

	return 0;
}