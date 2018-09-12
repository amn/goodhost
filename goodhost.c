#include <assert.h>
#include <crtdbg.h>
#include <io.h>
#include <stdio.h>
#include <windows.h>

struct parse_command_line_result {
	LPSTR log_file_path;
	LPSTR target_command_line;
};

struct find_whitespace_result {
	LPTSTR start;
	LPTSTR end;
};

static FILE * log_file;

static int find_whitespace(LPTSTR, struct find_whitespace_result *);
static int parse_command_line(LPSTR, struct parse_command_line_result *);

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	struct parse_command_line_result args;
	
	if (parse_command_line(lpCmdLine, &args) != 0) {
		MessageBox(NULL, "Error parsing command line arguments, aborting.", NULL, MB_ICONERROR);
		return -1;
	}

	SECURITY_ATTRIBUTES sa = { 0 };

	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	log_file = _fsopen(args.log_file_path, "a", _SH_DENYWR);

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

	/*hLogFile = CreateFile(args.log_file_path, GENERIC_WRITE, FILE_SHARE_READ, &sa, OPEN_EXISTING, 0, NULL);
	if (hLogFile == INVALID_HANDLE_VALUE) {
		_RPTF0(_CRT_ERROR, "Error opening log file for writing.\n");
		return -1;
	}

	const LARGE_INTEGER zero_large_offset = { 0 };
	assert(SetFilePointerEx(hLogFile, zero_large_offset, NULL, FILE_END));

	DWORD nWritten;*/

	fprintf(log_file, "Creating child process '%s'. Observe its output below until its termination.\n", args.target_command_line);

	STARTUPINFO si = { 0 };

	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;

	si.hStdOutput = si.hStdError = (HANDLE)_get_osfhandle(_fileno(log_file));

	assert(si.hStdOutput != INVALID_HANDLE_VALUE);

	PROCESS_INFORMATION pi = { 0 };

	fflush(log_file);

	if (!CreateProcess(NULL, args.target_command_line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
		fprintf(log_file, "Error creating child process (%d).\n", GetLastError());
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

	return err;
}

int parse_command_line(LPSTR lpCmdLine, struct parse_command_line_result * p_parse_command_line_result)
{
	struct find_whitespace_result ws;

	p_parse_command_line_result->log_file_path = lpCmdLine;

	if (find_whitespace(lpCmdLine, &ws) != 0) {
		return -1;
	}

	*(ws.start) = '\0';

	if (*(ws.end) == '\0') {
		return -2;
	}

	p_parse_command_line_result->target_command_line = ws.end;
	
	return 0;
}

int find_whitespace(LPTSTR s, struct find_whitespace_result * p_result)
{
	for (; *s != '\0'; s++) {
		if (isspace(*s)) {
			p_result->start = s;
			s++;
			for (; isspace(*s); s++);
			p_result->end = s;
			return 0;
		}
	}
	return -1;
}	