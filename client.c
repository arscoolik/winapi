#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <conio.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 4096
#define DEFAULT_PORT "29015"
#define EXIT_COMMAND "exit\n"

int main(int argc, char** argv) {
    WSADATA wsaData;
    SOCKET clientSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL;
    struct addrinfo* ptr = NULL;
    struct addrinfo hints;
    char sendBuffer[DEFAULT_BUFLEN];
    char recvBuffer[DEFAULT_BUFLEN];
    int iResult;

    if (argc != 2) {
        printf("Usage: %s server-name\n", argv[0]);
        return 1;
    }

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo(argv[1], DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        exit(1);
    }

    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        clientSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (clientSocket == INVALID_SOCKET) {
            printf("socket failed with error: %d\n", WSAGetLastError());
            WSACleanup();
            exit(1);
        }

        iResult = connect(clientSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
        }
    }

    freeaddrinfo(result);

    if (clientSocket == INVALID_SOCKET) {
        printf("Unable to connect to the server\n");
        WSACleanup();
        exit(1);
    }

    printf("Type 'exit' to end the session\n");

    do {
        int i = 0;
        do {
            char c = (char)_getch();
            sendBuffer[i] = c;
            if (i == DEFAULT_BUFLEN - 3) {
                sendBuffer[i + 1] = '\n';
                sendBuffer[i + 2] = '\0';
                printf("\nNOTE: Maximum buffer length reached\n");
                break;
            }
            if (c == '\r') {
                printf("\n");
                sendBuffer[i] = '\n';
                sendBuffer[i + 1] = '\0';
                break;
            }
            else if (c == '\b' && i > 0) {
                sendBuffer[i] = '\0';
                sendBuffer[i - 1] = '\0';
                i -= 2;
                printf("\n%s", sendBuffer);
                continue;
            }
            printf("%c", c);
        } while (1);

        iResult = send(clientSocket, sendBuffer, i + 1, 0);
        if (iResult == SOCKET_ERROR) {
            printf("send failed with error: %d\n", WSAGetLastError());
            closesocket(clientSocket);
            WSACleanup();
            exit(1);
        }
        printf("Bytes Sent: %d\n", iResult);

        iResult = recv(clientSocket, recvBuffer, DEFAULT_BUFLEN, 0);
        if (iResult > 0)
            printf("Bytes received: %d\n", iResult);
        else if (iResult == 0)
            printf("Connection closed\n");
        else
            printf("recv failed with error: %d\n", WSAGetLastError());
        printf("\n%s", recvBuffer);
    } while (iResult > 0 && (strcmp(recvBuffer, EXIT_COMMAND) != 0));

    iResult = shutdown(clientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
