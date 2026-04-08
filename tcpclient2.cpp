/* ------------- tcpclient.cpp  ------------- */

#include <cstdio>
#include <cstring>
#include <conio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define HOST "192.168.10.23"
#define PORT "1259"

/* print bytes in HEX */
void printHex(const unsigned char* data, int length)
{
    for (int i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

/* connect to camera */
bool connectToServer(SOCKET& sockfd)
{
    struct addrinfo hints;
    struct addrinfo* servinfo = NULL;
    int rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rv = getaddrinfo(HOST, PORT, &hints, &servinfo);
    if (rv != 0) {
        printf("getaddrinfo failed\n");
        return false;
    }

    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sockfd == INVALID_SOCKET) {
        printf("Socket failed\n");
        freeaddrinfo(servinfo);
        return false;
    }

    if (connect(sockfd, servinfo->ai_addr, (int)servinfo->ai_addrlen) == SOCKET_ERROR) {
        printf("Connection failed: %d\n", WSAGetLastError());
        closesocket(sockfd);
        sockfd = INVALID_SOCKET;
        freeaddrinfo(servinfo);
        return false;
    }

    freeaddrinfo(servinfo);
    printf("Connected!\n");
    return true;
}

/* send VISCA payload */
bool sendPayload(SOCKET sockfd, const unsigned char* payload, int length)
{
    int bytesSent = send(sockfd, (const char*)payload, length, 0);

    if (bytesSent == SOCKET_ERROR) {
        printf("Send failed: %d\n", WSAGetLastError());
        return false;
    }

    printf("Payload sent: ");
    printHex(payload, length);
    return true;
}

/* receive response */
bool tryReceiveResponse(SOCKET sockfd)
{
    unsigned char buffer[100];
    int bytesReceived = recv(sockfd, (char*)buffer, sizeof(buffer), 0);

    if (bytesReceived > 0) {
        printf("Camera replied: ");
        printHex(buffer, bytesReceived);
        return true;
    }
    else if (bytesReceived == 0) {
        printf("Connection closed\n");
        return false;
    }
    else {
        printf("recv failed: %d\n", WSAGetLastError());
        return false;
    }
}

int main(void)
{
    WSADATA wsaData;
    SOCKET sockfd = INVALID_SOCKET;

    bool isMoving = false;   
    int speed = 6;           

    /* VISCA commands */
    unsigned char CMD_UP[] = { 0x81, 0x01, 0x06, 0x01, 0x01, 0x01, 0x03, 0x01, 0xFF };
    unsigned char CMD_DOWN[] = { 0x81, 0x01, 0x06, 0x01, 0x01, 0x01, 0x03, 0x02, 0xFF };
    unsigned char CMD_LEFT[] = { 0x81, 0x01, 0x06, 0x01, 0x01, 0x01, 0x01, 0x03, 0xFF };
    unsigned char CMD_RIGHT[] = { 0x81, 0x01, 0x06, 0x01, 0x01, 0x01, 0x02, 0x03, 0xFF };
    unsigned char CMD_STOP[] = { 0x81, 0x01, 0x06, 0x01, 0x01, 0x01, 0x03, 0x03, 0xFF };

    printf("Starting program...\n");

    /* STEP 1: Start Winsock */
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    /* STEP 2: Connect to camera */
    if (!connectToServer(sockfd)) {
        WSACleanup();
        return 1;
    }

    printf("\nControls:\n");
    printf("  Arrow keys = move camera\n");
    printf("  S          = stop camera\n");
    printf("  +          = increase speed (only when stopped)\n");
    printf("  -          = decrease speed (only when stopped)\n");
    printf("  ESC        = exit\n\n");

    printf("Current speed: %d\n\n", speed);

    /* STEP 3: Main loop */
    while (1)
    {
        int key = _getch();

        /* ESC = exit */
        if (key == 27) {
            printf("Exiting client...\n");
            break;
        }

        /* + increase speed (o nly if stopped) */
        if (key == '+') {
            if (isMoving) {
                printf("Cannot change speed while moving. Press S first.\n");
                continue;
            }

            if (speed < 0x14) {
                speed++;
                printf("Speed increased to: %d\n", speed);
            }
            else {
                printf("Speed is already max\n");
            }
            continue;
        }

        /* - decrease speed (only if stopped) */
        if (key == '-') {
            if (isMoving) {
                printf("Cannot change speed while moving. Press S first.\n");
                continue;
            }

            if (speed > 0x01) {
                speed--;
                printf("Speed decreased to: %d\n", speed);
            }
            else {
                printf("Speed is already min\n");
            }
            continue;
        }

        /* S = STOP */
        if (key == 's' || key == 'S') {
            if (!isMoving) {
                printf("Already stopped\n");
                continue;
            }

            CMD_STOP[4] = (unsigned char)speed;
            CMD_STOP[5] = (unsigned char)speed;

            if (!sendPayload(sockfd, CMD_STOP, sizeof(CMD_STOP))) break;
            if (!tryReceiveResponse(sockfd)) break;

            isMoving = false;
            printf("Camera stopped\n");
            continue;
        }

        /* Arrow keys */
        if (key == 224) {
            int arrow = _getch();

            if (isMoving) {
                printf("Already moving. Press S first.\n");
                continue;
            }

            /* update speed bytes */
            CMD_UP[4] = CMD_DOWN[4] = CMD_LEFT[4] = CMD_RIGHT[4] = (unsigned char)speed;
            CMD_UP[5] = CMD_DOWN[5] = CMD_LEFT[5] = CMD_RIGHT[5] = (unsigned char)speed;

            const unsigned char* payload = NULL;
            int len = 0;

            if (arrow == 72) { payload = CMD_UP; len = sizeof(CMD_UP); printf("UP\n"); }
            else if (arrow == 80) { payload = CMD_DOWN; len = sizeof(CMD_DOWN); printf("DOWN\n"); }
            else if (arrow == 75) { payload = CMD_LEFT; len = sizeof(CMD_LEFT); printf("LEFT\n"); }
            else if (arrow == 77) { payload = CMD_RIGHT; len = sizeof(CMD_RIGHT); printf("RIGHT\n"); }

            if (payload != NULL) {
                if (!sendPayload(sockfd, payload, len)) break;
                if (!tryReceiveResponse(sockfd)) break;

                isMoving = true;  
            }
        }
    }

    /* STEP 4: Cleanup */
    if (sockfd != INVALID_SOCKET) {
        closesocket(sockfd);
    }

    WSACleanup();

    printf("Program finished.\n");
    return 0;
}