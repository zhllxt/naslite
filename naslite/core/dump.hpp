#pragma once

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64) || defined(_WINDOWS_)

#if defined(_MSC_VER)
#	pragma warning(push) 
#	pragma warning(disable:4091)
#	pragma warning(disable:4996)
#endif

#include <Windows.h>
#include <tchar.h>
#include <Dbghelp.h>
#pragma comment(lib,"Dbghelp.lib")

void CreateDumpFile(LPCTSTR lpstrDumpFilePathName, EXCEPTION_POINTERS *pException)
{
	HANDLE hDumpFile = CreateFile(lpstrDumpFilePathName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hDumpFile)
	{
		MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
		dumpInfo.ExceptionPointers = pException;
		dumpInfo.ThreadId = GetCurrentThreadId();
		dumpInfo.ClientPointers = TRUE;

		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, &dumpInfo, NULL, NULL);

		CloseHandle(hDumpFile);
	}
}

LONG ApplicationCrashHandler(EXCEPTION_POINTERS *pException)
{
	TCHAR szExeFilePath[MAX_PATH]{ 0 };
	::GetModuleFileName(NULL, szExeFilePath, MAX_PATH);
	TCHAR* pszFileName = _tcsrchr(szExeFilePath, '\\');
	if (!pszFileName) pszFileName = _tcsrchr(szExeFilePath, '/');
	if (!pszFileName)
	{
		CreateDumpFile(_T("exception.dmp"), pException);
		FatalAppExit(-1, _T("Crashed"));
	}
	else
	{
		pszFileName++;

		TCHAR szDumpFileName[MAX_PATH]{ 0 };
		_tcscpy(szDumpFileName, pszFileName);
		TCHAR* pszDot = _tcschr(szDumpFileName, '.');
		if (pszDot) *pszDot = '\0';
		_tcscat(szDumpFileName, _T(".dmp"));
		CreateDumpFile(szDumpFileName, pException);

		TCHAR szMsg[MAX_PATH]{ 0 };
		_tcscpy(szMsg, pszFileName);
		_tcscat(szMsg, _T(" Crashed"));
		FatalAppExit(-1, szMsg);
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

bool InstallDumpHandler()
{
	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ApplicationCrashHandler);
	return true;
}

#if defined(_MSC_VER)
#	pragma warning(pop) 
#endif

#else

bool InstallDumpHandler()
{
	return false;
}

#endif // windows
