// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>

#include <stdio.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer layer = {
        .role = strcmp(role, "rx") ? LlTx : LlRx,
        .baudRate = baudRate,
        .nRetransmissions = nTries,
        .timeout = timeout
    };

    strcpy(layer.serialPort, serialPort);

    if (llopen(layer) != 1) {
        printf("Failed to do llopen\n");
    }


    // Sem estatísticas por agora
    if (llclose(0) == -1) {
        printf("Failed to do llclose\n");
    }
}
