// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>
#include <stdlib.h>

#include <stdio.h>

int sendFile(const char *filename)
{

    // Test
    unsigned char frame1[10] = {0x80, 0x7d, 0x7e, 0x41, 0x7d, 0x40, 0x40, 0x7e, 0x40, 0x40};
    unsigned char frame2[9] = {0x41, 0x7d, 0x7e, 0x42, 0x7d, 0x41, 0x41, 0x01, 0x41};
    unsigned char frame3[11] = {0x42, 0x7d, 0x7e, 0x43, 0x7d, 0x42, 0x42, 0x7e, 0x42, 0x41, 0x7d};
    unsigned char frame4[9] = {0x43, 0x7d, 0x7e, 0x44, 0x35, 0x43, 0x43, 0x7e, 0x43};

    if (llwrite(frame1, 10) <= 0)
    {
        printf("Error!\n");
    }

    if (llwrite(frame2, 9) <= 0)
    {
        printf("Error!\n");
    }
    if (llwrite(frame3, 11) <= 0)
    {
        printf("Error!\n");
    }
    if (llwrite(frame4, 9) <= 0)
    {
        printf("Error!\n");
    }

    return 0;
}

int receiveFile(const char *filename)
{

    // Test
    unsigned char packet1[50];
    unsigned char packet2[50];
    unsigned char packet3[50];
    unsigned char packet4[50];

    int l = llread(packet1);

    if (l == 0)
    {
        printf("Error!\n");
    }
    printf("Packet: \n");
    for (int i = 0; i < l; i++)
    {
        printf("%02X ", packet1[i]);
    }
    printf("\n");


    int l1 = llread(packet2);
    if (l1 == 0)
    {
        printf("Error!\n");
    }
    printf("Packet: \n");
    for (int i = 0; i < l1; i++)
    {
        printf("%02X ", packet2[i]);
    }
    printf("\n");

    int l2 = llread(packet3);
    if (l2 == 0)
    {
        printf("Error!\n");
    }
    printf("Packet: \n");
    for (int i = 0; i < l2; i++)
    {
        printf("%02X ", packet3[i]);
    }
    printf("\n");

    int l4 = llread(packet4);
    if (l4 == 0)
    {
        printf("Error!\n");
    }
    printf("Packet: \n");
    for (int i = 0; i < l4; i++)
    {
        printf("%02X ", packet4[i]);
    }
    printf("\n");


    return 0;
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer layer = {
        .role = strcmp(role, "rx") ? LlTx : LlRx,
        .baudRate = baudRate,
        .nRetransmissions = nTries,
        .timeout = timeout};

    strcpy(layer.serialPort, serialPort);

    if (llopen(layer) != 1)
    {
        printf("Failed to do llopen\n");
        exit(-1);
    }

    switch (layer.role)
    {
    case LlTx:
        if (sendFile(filename) == -1)
        {
            printf("Failed to sendFile\n");
            exit(-1);
        }
        break;
    case LlRx:
        if (receiveFile(filename) == -1)
        {
            printf("Failed to receiveFile\n");
            exit(-1);
        }
        break;
    default:
        break;
    }

    // Sem estatísticas por agora
    if (llclose(1) == -1)
    {
        printf("Failed to do llclose\n");
        exit(-1);
    }
}
