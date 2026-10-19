// RCOM 2026/2027
//
// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 256

#define FALSE 0
#define TRUE 1

#define FLAG 0x7E
#define A_TX 0x03
#define A_RX 0x01
#define C_SET 0x03
#define C_UA 0x07

typedef enum
{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP
} State;

int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d received\n", alarmCount);
}

State stateMachine(State state, unsigned char byte, unsigned char a, unsigned char c)
{
    switch (state)
    {
    case START:
        if (byte == FLAG)
            state = FLAG_RCV;
        break;
    case FLAG_RCV:
        if (byte == a)
            state = A_RCV;
        else if (byte != FLAG)
            state = START;
        break;
    case A_RCV:
        if (byte == c)
            state = C_RCV;
        else if (byte == FLAG)
            state = FLAG_RCV;
        else
            state = START;
        break;
    case C_RCV:
        if (byte == (a ^ c))
            state = BCC_OK;
        else if (byte == FLAG)
            state = FLAG_RCV;
        else
            state = START;
        break;
    case BCC_OK:
        if (byte == FLAG)
            state = STOP;
        else
            state = START;
        break;
    default:
        break;
    }

    return state;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llOpenTx(LinkLayer llParameters)
{
    // ----------------------------------------------------
    // This example code shows how to open the serial port and send a string.
    // TODO: Adapt and extend this code according to the specifications of the project.
    // ----------------------------------------------------

    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);

    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

    unsigned char buf[5] = {FLAG, A_TX, C_SET, A_TX ^ C_SET, FLAG};
    State state = START;

    while (state != STOP && alarmCount <= llParameters.nRetransmissions)
    {
        if (alarmEnabled == FALSE)
        {
            int bytes = writeBytesSerialPort(buf, 5);
            printf("SET sent (%d bytes)\n", bytes);

            alarm(llParameters.timeout);
            alarmEnabled = TRUE;
        }

        unsigned char byte;
        int bytes = readByteSerialPort(&byte);

        if (bytes > 0)
        {
            printf("Byte received: 0x%02X\n", byte);
            state = stateMachine(state, byte, A_TX, C_UA);
        }
    }

    alarm(0);

    if (state != STOP)
    {
        return -1;
    }

    printf("UA received\n");

    return 0;
}

int llOpenRx(LinkLayer llParameters)
{
    // ----------------------------------------------------
    // This example code shows how to open the serial port and receive a string.
    // TODO: Adapt and extend this code according to the specifications of the project.
    // ----------------------------------------------------

    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);

    State state = START;

    while (state != STOP)
    {
        // Read one byte from serial port.
        // NOTE: You must check how many bytes were actually read by reading the return value.
        unsigned char byte;
        int bytes = readByteSerialPort(&byte);

        if (bytes < 0)
        {
            return -1;
        }
        if (bytes == 0)
        {
            continue;
        }

        printf("Byte received: 0x%02X\n", byte);
        state = stateMachine(state, byte, A_TX, C_SET);
    }

    printf("SET received\n");

    unsigned char buf[5] = {FLAG, A_TX, C_UA, A_TX ^ C_UA, FLAG};

    int bytes = writeBytesSerialPort(buf, 5);
    printf("UA sent (%d bytes)\n", bytes);

    return 0;
}

////////////////////////////////////////////////
// LLSEND
////////////////////////////////////////////////
int llSend(const unsigned char *buf, int bufSize)
{
    // TODO: Implement this function

    return 0;
}

////////////////////////////////////////////////
// LLRECEIVE
////////////////////////////////////////////////
int llReceive(unsigned char *packet)
{
    // TODO: Implement this function

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llCloseTx()
{
    // TODO: Implement this function

    return 0;
}

int llCloseRx()
{
    // TODO: Implement this function

    return 0;
}
