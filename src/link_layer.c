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
#define C_DISC 0x0B

#define C_RR0 0xAA
#define C_RR1 0xAB
#define C_REJ0 0x54
#define C_REJ1 0x55
#define C_I0 0x00
#define C_I1 0x80
#define ESC 0x7D

typedef enum
{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    DATA,
    STOP
} State;

int alarmEnabled = FALSE;
int alarmCount = 0;

int nRetransmissions = 0;
int timeout = 0;

int Ns = 0;
int Nr = 0;

int framesSent = 0;
int retransmissions = 0;
int timeouts = 0;
int rejReceived = 0;

int framesReceived = 0;
int rejSent = 0;
int duplicates = 0;

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

int receiveResponse()
{
    State state = START;
    unsigned char c = 0;

    while (state != STOP)
    {
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

        switch (state)
        {
        case START:
            if (byte == FLAG)
                state = FLAG_RCV;
            break;
        case FLAG_RCV:
            if (byte == A_TX)
                state = A_RCV;
            else if (byte != FLAG)
                state = START;
            break;
        case A_RCV:
            if (byte == C_RR0 || byte == C_RR1 || byte == C_REJ0 || byte == C_REJ1)
            {
                c = byte;
                state = C_RCV;
            }
            else if (byte == FLAG)
                state = FLAG_RCV;
            else
                state = START;
            break;
        case C_RCV:
            if (byte == (A_TX ^ c))
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
    }

    return c;
}

void sendRR()
{
    unsigned char c = C_RR0;
    if (Nr == 1)
    {
        c = C_RR1;
    }

    unsigned char buf[5] = {FLAG, A_TX, c, A_TX ^ c, FLAG};
    writeBytesSerialPort(buf, 5);
}

void sendREJ()
{
    unsigned char c = C_REJ0;
    if (Nr == 1)
    {
        c = C_REJ1;
    }

    unsigned char buf[5] = {FLAG, A_TX, c, A_TX ^ c, FLAG};
    writeBytesSerialPort(buf, 5);
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

    nRetransmissions = llParameters.nRetransmissions;
    timeout = llParameters.timeout;

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

    nRetransmissions = llParameters.nRetransmissions;
    timeout = llParameters.timeout;

    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

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
    unsigned char frame[2 * MAX_PAYLOAD_SIZE + 7];
    unsigned char c = C_I0;
    if (Ns == 1)
    {
        c = C_I1;
    }

    int size = 0;
    frame[size++] = FLAG;
    frame[size++] = A_TX;
    frame[size++] = c;
    frame[size++] = A_TX ^ c;

    unsigned char bcc2 = 0;
    for (int i = 0; i < bufSize; i++)
    {
        bcc2 ^= buf[i];

        if (buf[i] == FLAG || buf[i] == ESC)
        {
            frame[size++] = ESC;
            frame[size++] = buf[i] ^ 0x20;
        }
        else
        {
            frame[size++] = buf[i];
        }
    }

    if (bcc2 == FLAG || bcc2 == ESC)
    {
        frame[size++] = ESC;
        frame[size++] = bcc2 ^ 0x20;
    }
    else
    {
        frame[size++] = bcc2;
    }

    frame[size++] = FLAG;

    unsigned char rr = C_RR1;
    unsigned char rej = C_REJ0;
    if (Ns == 1)
    {
        rr = C_RR0;
        rej = C_REJ1;
    }

    int bytes = 0;
    alarmCount = 0;
    alarmEnabled = FALSE;

    while (alarmCount <= nRetransmissions)
    {
        if (alarmEnabled == FALSE)
        {
            bytes = writeBytesSerialPort(frame, size);
            printf("I frame sent (%d bytes)\n", bytes);

            framesSent++;
            if (alarmCount > 0)
            {
                retransmissions++;
            }

            alarm(timeout);
            alarmEnabled = TRUE;
        }

        int response = receiveResponse();

        if (response == rr)
        {
            alarm(0);
            printf("RR received\n");
            Ns = 1 - Ns;
            return bytes;
        }

        if (response == rej)
        {
            alarm(0);
            printf("REJ received\n");
            alarmEnabled = FALSE;
            alarmCount++;
            rejReceived++;
        }
        else if (alarmEnabled == FALSE)
        {
            timeouts++;
        }
    }

    return -1;
}

////////////////////////////////////////////////
// LLRECEIVE
////////////////////////////////////////////////
int llReceive(unsigned char *packet)
{
    unsigned char frame[MAX_PAYLOAD_SIZE + 1];

    while (TRUE)
    {
        State state = START;
        unsigned char c = 0;
        int size = 0;
        int escape = FALSE;

        while (state != STOP)
        {
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

            switch (state)
            {
            case START:
                if (byte == FLAG)
                    state = FLAG_RCV;
                break;
            case FLAG_RCV:
                if (byte == A_TX)
                    state = A_RCV;
                else if (byte != FLAG)
                    state = START;
                break;
            case A_RCV:
                if (byte == C_I0 || byte == C_I1)
                {
                    c = byte;
                    state = C_RCV;
                }
                else if (byte == FLAG)
                    state = FLAG_RCV;
                else
                    state = START;
                break;
            case C_RCV:
                if (byte == (A_TX ^ c))
                {
                    size = 0;
                    escape = FALSE;
                    state = DATA;
                }
                else if (byte == FLAG)
                    state = FLAG_RCV;
                else
                    state = START;
                break;
            case DATA:
                if (byte == FLAG)
                    state = STOP;
                else if (size > MAX_PAYLOAD_SIZE)
                    state = START;
                else if (byte == ESC)
                    escape = TRUE;
                else if (escape)
                {
                    frame[size++] = byte ^ 0x20;
                    escape = FALSE;
                }
                else
                    frame[size++] = byte;
                break;
            default:
                break;
            }
        }

        int n = 0;
        if (c == C_I1)
        {
            n = 1;
        }

        unsigned char bcc2 = 0;
        for (int i = 0; i < size - 1; i++)
        {
            bcc2 ^= frame[i];
        }

        if (size > 0 && bcc2 == frame[size - 1] && n == Nr)
        {
            Nr = 1 - Nr;
            sendRR();
            framesReceived++;
            printf("I frame received, RR sent\n");

            for (int i = 0; i < size - 1; i++)
            {
                packet[i] = frame[i];
            }
            return size - 1;
        }
        else if (n == Nr)
        {
            sendREJ();
            printf("I frame with error, REJ sent\n");
            rejSent++;
        }
        else
        {
            sendRR();
            printf("Duplicate I frame, RR sent\n");
            duplicates++;
        }
    }
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llCloseTx()
{
    unsigned char buf[5] = {FLAG, A_TX, C_DISC, A_TX ^ C_DISC, FLAG};
    State state = START;
    alarmCount = 0;
    alarmEnabled = FALSE;

    while (state != STOP && alarmCount <= nRetransmissions)
    {
        if (alarmEnabled == FALSE)
        {
            int bytes = writeBytesSerialPort(buf, 5);
            printf("DISC sent (%d bytes)\n", bytes);

            alarm(timeout);
            alarmEnabled = TRUE;
        }

        unsigned char byte;
        int bytes = readByteSerialPort(&byte);

        if (bytes > 0)
        {
            state = stateMachine(state, byte, A_RX, C_DISC);
        }
    }

    alarm(0);

    if (state == STOP)
    {
        printf("DISC received\n");

        unsigned char ua[5] = {FLAG, A_RX, C_UA, A_RX ^ C_UA, FLAG};
        int bytes = writeBytesSerialPort(ua, 5);
        printf("UA sent (%d bytes)\n", bytes);
    }

    printf("I frames sent: %d\n", framesSent);
    printf("Retransmissions: %d\n", retransmissions);
    printf("Timeouts: %d\n", timeouts);
    printf("REJ received: %d\n", rejReceived);

    closeSerialPort();

    if (state != STOP)
    {
        return -1;
    }

    return 0;
}

int llCloseRx()
{
    State state = START;

    while (state != STOP)
    {
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

        state = stateMachine(state, byte, A_TX, C_DISC);
    }

    printf("DISC received\n");

    unsigned char buf[5] = {FLAG, A_RX, C_DISC, A_RX ^ C_DISC, FLAG};
    state = START;
    alarmCount = 0;
    alarmEnabled = FALSE;

    while (state != STOP && alarmCount <= nRetransmissions)
    {
        if (alarmEnabled == FALSE)
        {
            int bytes = writeBytesSerialPort(buf, 5);
            printf("DISC sent (%d bytes)\n", bytes);

            alarm(timeout);
            alarmEnabled = TRUE;
        }

        unsigned char byte;
        int bytes = readByteSerialPort(&byte);

        if (bytes > 0)
        {
            state = stateMachine(state, byte, A_RX, C_UA);
        }
    }

    alarm(0);

    if (state == STOP)
    {
        printf("UA received\n");
    }

    printf("I frames received: %d\n", framesReceived);
    printf("REJ sent: %d\n", rejSent);
    printf("Duplicates: %d\n", duplicates);

    closeSerialPort();

    if (state != STOP)
    {
        return -1;
    }

    return 0;
}
