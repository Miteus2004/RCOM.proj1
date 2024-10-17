// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "projFlags.h"
#include <stdio.h>
#include <signal.h>
#include <unistd.h>



// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

LinkLayer parameters;
State state = START;
extern int fd;
volatile int waitAlarm = FALSE;
int alarmCount = 0;

void alarmHandler(int signal)
{
    waitAlarm = FALSE;
    alarmCount++;
    printf("Couldnt receive UA frame\n");
}

void alarmInit() {
    waitAlarm = TRUE;
    alarm(parameters.timeout);
}

void handleTxState(unsigned char byte) {
    switch (state) {
        case START:
            if (byte == FLAG) {
                state = FLAG_RCV;
            } else {
                state = START;
            }
            break;
        case FLAG_RCV:
            if (byte == A_TRANS) {
                state = A_RCV;
            } else if (byte != FLAG) {
                state = START;
            }
            break;
        case A_RCV:
            if (byte == C_UA) {
                state = C_RCV;
            } else if (byte == FLAG) {
                state = FLAG_RCV;
            } else {
                state = START;
            }
            break;
        case C_RCV:
            if (byte == (A_TRANS ^ C_UA)) {
                state = BCC_OK;
            } else if (byte == FLAG) {
                state = FLAG_RCV;
            } else {
                state = START;
            }
            break;
        case BCC_OK:
            if (byte == FLAG) {
                state = STOP_STATE;
                printf("Frame received!\n");
            }
            break;
        case STOP_STATE:
            state = START;
            break;
    }
}


void handleRxState(unsigned char byte) {
    switch (state) {
        case START:
            if (byte == FLAG) {
                state = FLAG_RCV;
            } else {
                state = START;
            }
            break;
        case FLAG_RCV:
            if (byte == A_TRANS) {
                state = A_RCV;
            } else if (byte != FLAG) {
                state = START;
            }
            break;
        case A_RCV:
            if (byte == C_SET) {
                state = C_RCV;
            } else if (byte == FLAG) {
                state = FLAG_RCV;
            } else {
                state = START;
            }
            break;
        case C_RCV:
            if (byte == (A_TRANS ^ C_SET)) {
                state = BCC_OK;
            } else if (byte == FLAG) {
                state = FLAG_RCV;
            } else {
                state = START;
            }
            break;
        case BCC_OK:
            if (byte == FLAG) {
                state = STOP_STATE;
                printf("Frame received!\n");
            }
            break;
        case STOP_STATE:
            state = START;
            break;
    }
}



int transmitterSETframe() {
    (void)signal(SIGALRM, alarmHandler);
    int bytesSent = 0;
    unsigned char response;
    unsigned char frame[5] = {FLAG, A_TRANS, C_SET, A_TRANS ^ C_SET, FLAG};
    printf("Sending SENT frame\n");
    

    while (alarmCount < parameters.nRetransmissions) {
        if (!waitAlarm) {
            bytesSent = write(fd, frame, 5);
            printf("Written bytes on frame: %d\n", bytesSent);
            if (bytesSent != 5) {
                perror("Error writing frame");
                return -1;
            }
            alarmInit();
        }

        int bytesResponse = read(fd, &response, 1);

        if (bytesResponse != 0) {
            handleTxState(response);
        }

        if (state == STOP_STATE) {
            alarm(0);
            printf("UA frame received\n");
            return 1;
        }
    }

    return alarmCount != parameters.nRetransmissions;
}

int receiverSETframe() {
    while (TRUE) {
        unsigned char byteFrame;

        int byteRead = read(fd, &byteFrame, 1);

        if (byteRead != 0) {
            handleRxState(byteFrame);
        }

        if (state == STOP_STATE) {
            unsigned char sendFrame[5] = {FLAG, A_TRANS, C_UA, A_TRANS ^ C_UA, FLAG};

            handleRxState(byteFrame);

            int writeBytes = write(fd, sendFrame, 5);
            printf("Bytes written: %d\n", writeBytes);

            break;
        }
    }
    return 0;
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    parameters = connectionParameters;
    fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate);

    if (fd < 0) {
        return -1;
    }

    switch (connectionParameters.role)
    {
    case LlTx:
        transmitterSETframe();
        break;
    case LlRx:
        receiverSETframe();
        break;
    default:
        break;
    }

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    int clstat = closeSerialPort();
    return clstat;
}
