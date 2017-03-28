#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

/* xAP constants */
#define XAP_VERSION 12
#define XAP_PORT 3639
#define SIZEXAPADDR 136
#define SIZEXAPMESS 512

/* xAP globals */
struct sockaddr_in sckxapAddr;
char strlocalIPaddr[16];
char strUID[9];
char strSource[SIZEXAPADDR];
int xapHBeatPeriod = 60;
int fdSocket;
time_t xAPheartbTime = 0;

extern char node_id[];

void xapInit()
{
        fdSocket = xapSetup("");
        sprintf(strUID, "FF690000");
        sprintf(strSource, "DB.UKHASnet.%s", node_id);
}

short int xapSendPacket(char *Packet, int Rssi)
{
        char strMessage[SIZEXAPMESS];

        sckxapAddr.sin_family = AF_INET;
        sckxapAddr.sin_addr.s_addr = INADDR_BROADCAST;
        sckxapAddr.sin_port = htons(XAP_PORT);
        memset(&(sckxapAddr.sin_zero), '\0', 8);

        if (Rssi==0)
        {
                sprintf(strMessage, "xap-header\n{\nv=%d\nhop=1\nuid=%s\nclass=UKHASnet.gw\nsource=%s\n}\ntx.raw\n{\npacket=%s\n}\n",
                                XAP_VERSION, strUID, strSource, Packet);
        } else {

                sprintf(strMessage, "xap-header\n{\nv=%d\nhop=1\nuid=%s\nclass=UKHASnet.gw\nsource=%s\n}\nrx.raw\n{\npacket=%s\nrssi=%d\n}\n",
                                XAP_VERSION, strUID, strSource, Packet, Rssi);
        }

        // Send the message
        if (sendto(fdSocket, strMessage, strlen(strMessage), 0, (struct sockaddr*)&sckxapAddr, sizeof(sckxapAddr)) < 0)
                return 0;

        return 1;
}

void xapCheckHbeat()
{
        if (xapHBeatPeriod && (time(NULL) - xAPheartbTime) >= xapHBeatPeriod)
        {
                // Send a xAP heartbeat
                xapSendHbeat(fdSocket, strSource, strUID, xapHBeatPeriod);
                xAPheartbTime = time(NULL);     // Store current time for the next cycle
        }
}

