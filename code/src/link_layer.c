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
unsigned char control_flag = 0;
int currSeq = 0;

void alarmHandler(int signal)
{
    waitAlarm = FALSE;
    printf("Couldnt receive frame\n");
}

void alarmInit() {
    waitAlarm = TRUE;
    alarm(parameters.timeout);
}

void handleState(unsigned char byte) {
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
            if ((parameters.role == LlTx && (byte == C_UA || byte == C_RR0 || byte == C_RR1 || byte == C_REJ0 || byte == C_REJ1)) || 
                (parameters.role == LlRx && (byte == C_SET || byte == C_I0 || byte == C_I1))) {
                control_flag = byte;
                state = C_RCV;
            } else if (byte == FLAG) {
                state = FLAG_RCV;
            } else {
                state = START;
            }
            break;
        case C_RCV:
            if (byte == (A_TRANS ^ control_flag)) {
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
                printf("\nFrame received!\n");
            }
            break;
        case STOP_STATE:
        case DATA:
        case ESCAPE_STATE:
        default:
            break;
    }
}

int transmitterSETframe() {
    (void)signal(SIGALRM, alarmHandler);
    int bytesSent = 0;
    unsigned char response;
    unsigned char frame[5] = {FLAG, A_TRANS, C_SET, A_TRANS ^ C_SET, FLAG};
    printf("Sending SENT frame\n");
    
    int alarmCount = 0;
    while (alarmCount < parameters.nRetransmissions) {
        if (!waitAlarm) {
            alarmCount++;
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
            handleState(response);
        }

        if (state == STOP_STATE) {
            alarm(0);
            printf("UA frame received\n");
            return 1;
        }
    }

    return -1;
}

int receiverSETframe() {
    while (TRUE) {
        unsigned char byteFrame;

        int byteRead = read(fd, &byteFrame, 1);

        if (byteRead != 0) {
            handleState(byteFrame);
        }

        if (state == STOP_STATE) {
            unsigned char sendFrame[5] = {FLAG, A_TRANS, C_UA, A_TRANS ^ C_UA, FLAG};

            handleState(byteFrame);

            int writeBytes = write(fd, sendFrame, 5);
            printf("Bytes written: %d\n", writeBytes);

            return 1;
        }
    }
    return -1;
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
        if (transmitterSETframe() != 1) {
            return -1;
        };
        break;
    case LlRx:
        if (receiverSETframe() != 1) {
            return -1;
        };
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
    printf("Writting bytes...\n");

    unsigned char iframe[bufSize*3], bcc2 = 0, idx = 0;
    iframe[idx] = FLAG; idx++;
    iframe[idx] = A_TRANS; idx++;
    iframe[idx] = currSeq ? C_I1 : C_I0; idx++;
    iframe[idx] = A_TRANS ^ iframe[idx-1]; idx++;


    for (int i = 0; i < bufSize; i++) {
        unsigned char currByte = buf[i];

        switch (currByte) {
            case ESCAPE:
                iframe[idx] = ESCAPE; idx++;
                iframe[idx] = ESCAPE ^ 0x20; idx++;
                break;
            case FLAG:
                iframe[idx] = ESCAPE; idx++;
                iframe[idx] = FLAG ^ 0x20; idx++;
                break;
            default:
                iframe[idx] = currByte; idx++;
                break;
        }

        bcc2 ^= currByte;
    }
    
    iframe[idx] = bcc2;

    if (bcc2 == FLAG) {
        iframe[idx] = ESCAPE; idx++;
        iframe[idx] = FLAG ^ 0x20;
    } else if (bcc2 == ESCAPE) {
        iframe[idx] = ESCAPE; idx++;
        iframe[idx] = ESCAPE ^ 0x20;
    }
    idx++;

    iframe[idx] = FLAG;

    for (int i = 0; i < idx +1; i++) {
        printf("%02x ", iframe[i]);
    }
    printf("\n");

    int alarmCount = 0;
    int bytesSent = 0;
    waitAlarm = FALSE;
    state = START;
    unsigned char response;
    while (alarmCount < parameters.nRetransmissions) {

        if (!waitAlarm) {
            alarmCount++;
            bytesSent = write(fd, iframe, idx+1);
            printf("Written bytes on frame: %d\n", bytesSent);
            
            alarmInit();
        }

        int bytesResponse = read(fd, &response, 1);

        if (bytesResponse > 0) {
            printf("%02X ", response);
            handleState(response);
        }

        if (state == STOP_STATE && control_flag == (currSeq? C_RR0 : C_RR1)) {
            alarm(0);
            printf("Info frame received\n");
            currSeq = 1 - currSeq;
            printf("New currSeq: %d\n", currSeq);
            return idx;
        }
    }

    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    State state_read = START;
    unsigned char control_byte, byte;
    int idx = 0;

    while (state_read != STOP_STATE) {

        int byteRead = read(fd, &byte, 1);

        if (byteRead > 0) {
            switch (state_read) {
            case START:
                if (byte == FLAG) {
                    state_read = FLAG_RCV;
                } else {
                    state_read = START;
                }
                break;
            case FLAG_RCV:
                if (byte == A_TRANS) {
                    state_read = A_RCV;
                } else if (byte != FLAG) {
                    state_read = START;
                }
                break;
            case A_RCV:
                if (byte == C_I0 || byte == C_I1) {
                    control_byte = byte;
                    state_read = C_RCV;
                } else if (byte == FLAG) {
                    state_read = FLAG_RCV;
                } else {
                    state_read = START;
                }
                break;
            case C_RCV:
                if (byte == (A_TRANS ^ control_byte)) {
                    state_read = BCC_OK;
                } else if (byte == FLAG) {
                    state_read = FLAG_RCV;
                } else {
                    state_read = START;
                }
                break;
            case BCC_OK:
                if (byte == FLAG) {
                    state_read = STOP_STATE;
                } else if (byte == ESCAPE) {
                    state_read = ESCAPE_STATE;
                } else {
                    packet[idx] = byte; idx++;
                    state_read = DATA;
                }
                break;
            case DATA:
                if (byte == ESCAPE) {
                    state_read = ESCAPE_STATE;
                } else if (byte == FLAG) {
                    state_read = STOP_STATE;
                } else {
                    packet[idx] = byte; idx++;
                }
                break;
            case ESCAPE_STATE:
                if (byte == (FLAG ^ 0x20)) {
                    state_read = DATA;
                    packet[idx] = FLAG; idx++;
                } else if (byte == (ESCAPE ^ 0x20)) {
                    state_read = DATA;
                    packet[idx] = ESCAPE; idx++;
                } else {
                    state_read = START;
                }
                break;
            case STOP_STATE:
            default:
                break;
            }   
        }
    }

    unsigned char bcc2 = 0;
    for (int i = 0; i < idx-1; i++) {
        bcc2 ^= packet[i];
    }

    unsigned char control_response;

    if (bcc2 != packet[idx-1]) {
        printf("Wrong bcc2!\n");

        control_response = currSeq ? C_REJ1 : C_REJ0;

        unsigned char response[5] = {FLAG, A_TRANS, control_response, A_TRANS ^ control_response, FLAG};
        write(fd, response, 5);
        return -1;
    }

    control_response = currSeq ? C_RR0 : C_RR1;
    unsigned char response[5] = {FLAG, A_TRANS, control_response, A_TRANS ^ control_response, FLAG};
    int writtenBytes = write(fd, response, 5);
    printf("Written bytes on response: %d\n", writtenBytes);
    currSeq = 1 - currSeq;
    printf("New currSeq: %d\n", currSeq);
    return idx;
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
