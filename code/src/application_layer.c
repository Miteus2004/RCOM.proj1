// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define PACKET_START    0x01
#define PACKET_DATA     0x02
#define PACKET_END      0x03
#define PACKET_FSIZE    0x00

int sendControlPacket(unsigned char control, size_t size) {
    unsigned char packet[MAX_PAYLOAD_SIZE] = {0};

    packet[0] = control;
    packet[1] = PACKET_FSIZE;
    packet[2] = 4;
    packet[3] = size >> 24;
    packet[4] = (size >> 16) & 0xFF;
    packet[5] = (size >> 8) & 0xFF;
    packet[6] = size & 0xFF;

    if ((llwrite(packet, 7) == -1)) {
        printf("Error sending control packet!\n");
        exit(-1);
    } else {
        printf("Control packet sent!\n");
    }

    return 0;
}

int receiveControlPacket(unsigned char control, size_t *size) {
    unsigned char packet[MAX_PAYLOAD_SIZE] = {0};
    llread(packet);

    if (packet[0] != control) {
        printf("Error receiving control packet on byte 0!\n");
        exit(-1);
    }

    if (packet[1] != PACKET_FSIZE) {
        printf("Error receiving control packeton byte 1!\n");
        exit(-1);
    }

    if (packet[2] != 4) {
        printf("Error receiving control packet on byte 2!\n");
        exit(-1);
    }

    *size = (packet[3] << 24) | (packet[4] << 16) | (packet[5] << 8) | packet[6];

    printf("Control packet received!\n");

    return 0;
}


int sendFile(const char *filename)
{
    FILE *file = fopen(filename, "rb");

    if (file == NULL) {
        printf("Error opening file!\n");
        exit(-1);
    }

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (sendControlPacket(PACKET_START, size) != 0) {
        printf("Error sending control packet!\n");
        fclose(file);
        exit(-1);
    }

    int packetNumber = 0, bytesRead = 0;
    unsigned char buffer[MAX_PAYLOAD_SIZE] = {0};

    while ((bytesRead = fread(buffer, 1, MAX_PAYLOAD_SIZE, file)) > 0) {
        printf("\nPacket Number: %d\n", packetNumber);
        unsigned char packet[MAX_PAYLOAD_SIZE + 4];
        packet[0] = PACKET_DATA;
        packet[1] = packetNumber;
        packet[2] = (bytesRead >> 8) & 0xFF;
        packet[3] = bytesRead & 0xFF;

        memcpy(&packet[4], buffer, bytesRead);

        if (llwrite(packet, bytesRead + 4) < 0) {
            printf("Error sending data packet!\n");
            fclose(file);
            exit(-1);
        }

        packetNumber = (packetNumber + 1) % 100;
    }


    if (sendControlPacket(PACKET_END, size) != 0) {
        printf("Error sending control packet!\n");
        fclose(file);
        exit(-1);
    }

    fclose(file);
    return 0;
}

int receiveFile(const char *filename)
{
    FILE *file = fopen(filename, "wb");

    if (file == NULL) {
        printf("Error opening file!\n");
        exit(-1);
    }

    size_t filesize = 0;

    if (receiveControlPacket(PACKET_START, &filesize) != 0) {
        printf("Error receiving control packet!\n");
        fclose(file);
        exit(-1);
    }

    unsigned char buffer[MAX_PAYLOAD_SIZE] = {0};
    size_t bytesWritten = 0;
    int packetNumber = 0;

    while (bytesWritten < filesize) {
        printf("\nPacket number: %d\n", packetNumber);
        unsigned char packet[MAX_PAYLOAD_SIZE + 4] = {0};
        int bytesSent = llread(packet);

        if (packet[0] != PACKET_DATA) {
            printf("Error receiving data packet!\n");
            fclose(file);
            exit(-1);
        }

        if (packet[1] != packetNumber) {
            printf("Error receiving data packet! Wrong packet number.\n");
            fclose(file);
            exit(-1);
        }

        if (bytesSent > 0) {
            int size = packet[2] * 256 + packet[3];
            memcpy(buffer, &packet[4], size);

            bytesWritten += fwrite(buffer, 1, size, file);
            printf("Written %d bytes\n", bytesWritten);

            packetNumber = (packetNumber + 1) % 100;
        }
    }

    if (receiveControlPacket(PACKET_END, &filesize) != 0) {
        printf("Error receiving control packet!\n");
        fclose(file);
        exit(-1);
    }

    fclose(file);
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

    if (llclose(1) == -1)
    {
        printf("Failed to do llclose\n");
        exit(-1);
    }
}
