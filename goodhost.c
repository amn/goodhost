#include <assert.h>
#include <crtdbg.h>
#include <io.h>
#include <stdio.h>
#include <windows.h>

LPSTR* WINAPI CommandLineToArgvA(LPSTR lpCmdline, int* numargs);

static FILE * log_file;

LPSTR load_subprocess_argv(LPSTR * argv, int argc);

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	LPSTR * argv;
	int argc;

	argv = CommandLineToArgvA(lpCmdLine, &argc);

	if (argv == NULL) {
		MessageBox(NULL, "Invalid command line.", NULL, MB_ICONERROR);
		return -1;
	}

	SECURITY_ATTRIBUTES sa = { 0 };

	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	log_file = _fsopen(argv[0], "a", _SH_DENYWR);

	if (log_file == NULL) {
		MessageBox(NULL, "Error opening log file for writing, aborting.", NULL, MB_ICONERROR);
		return -2;
	}

	int err = 0;

	DWORD dwUserNameLength = 1024;
	TCHAR szUserName[1024];

	assert(GetUserName(szUserName, &dwUserNameLength));

	TCHAR szModulePath[MAX_PATH];

	assert(GetModuleFileName(NULL, szModulePath, MAX_PATH));

	fprintf(log_file, "\nHello. This is Good Host, loaded from '%s' by user '%s'.\n", szModulePath, szUserName);

	STARTUPINFO si = { 0 };

	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;

	si.hStdOutput = si.hStdError = (HANDLE)_get_osfhandle(_fileno(log_file));

	assert(si.hStdOutput != INVALID_HANDLE_VALUE);

	PROCESS_INFORMATION pi = { 0 };

	LPSTR sp_argv = load_subprocess_argv(argv, argc);

	fprintf(log_file, "Creating child process '%s'. Observe its output below until its termination.\n", sp_argv);

	fflush(log_file);

	if (!CreateProcess(NULL, sp_argv, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
		fprintf(log_file, "Error creating child process (%d).\n", GetLastError());
		MessageBox(NULL, "Error creating child process.", NULL, MB_ICONERROR);
		err = -3;
	}

	if (pi.hProcess) {
		WaitForSingleObject(pi.hProcess, INFINITE);
		fflush(log_file);
		DWORD target_exit_code;
		if (!GetExitCodeProcess(pi.hProcess, &target_exit_code)) {
			fprintf(log_file, "Error getting child process exit code (%d).\n", GetLastError());
			err = -4;
		}
		else {
			fprintf(log_file, "Child process %d has exited with code %d.\n", pi.dwProcessId, target_exit_code);
		}
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	fprintf(log_file, "Closing this log file. Nothing else to do. Bye.\n");

	fclose(log_file);

	LocalFree(argv);

	free(sp_argv);

	return err;
}

LPSTR load_subprocess_argv(LPSTR * argv, int argc)
{
	const int skip = 1;
	size_t l = 0;

	for (int i = skip; i < argc; i++) {
		l += strlen(argv[i]) + 2;
	}

	char * p_buf = malloc(l + argc - 1 - skip + 1);
	char * dst = p_buf;

	*dst = 0;

	if (argc) {
		for (int i = skip; i < argc; i++) {
			*dst++ = '\"';
			for (char * src = argv[i]; *src; ) {
				*dst++ = *src++;
			}
			*dst++ = '\"';
			*dst++ = ' ';
		}
		*(dst - 1) = 0;
	}

	assert(dst == p_buf + l + argc - 1 - skip + 1);

	return p_buf;
}