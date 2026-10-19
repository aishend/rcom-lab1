// RCOM 2026/2027
//
// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <stdio.h>
#include <string.h>

int buildControlPacket(unsigned char *packet, unsigned char c, long fileSize, const char *filename)
{
    int size = 0;
    packet[size++] = c;

    packet[size++] = 0;
    packet[size++] = 4;
    packet[size++] = (fileSize >> 24) & 0xFF;
    packet[size++] = (fileSize >> 16) & 0xFF;
    packet[size++] = (fileSize >> 8) & 0xFF;
    packet[size++] = fileSize & 0xFF;

    int nameSize = strlen(filename);
    packet[size++] = 1;
    packet[size++] = nameSize;
    memcpy(&packet[size], filename, nameSize);
    size += nameSize;

    return size;
}

void sendFile(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        perror("fopen");
        return;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char packet[MAX_PAYLOAD_SIZE];

    int size = buildControlPacket(packet, 1, fileSize, filename);
    if (llSend(packet, size) < 0)
    {
        printf("Error sending START packet\n");
        fclose(file);
        return;
    }

    int bytes = fread(&packet[3], 1, MAX_PAYLOAD_SIZE - 3, file);
    while (bytes > 0)
    {
        packet[0] = 2;
        packet[1] = bytes / 256;
        packet[2] = bytes % 256;

        if (llSend(packet, bytes + 3) < 0)
        {
            printf("Error sending data packet\n");
            fclose(file);
            return;
        }

        bytes = fread(&packet[3], 1, MAX_PAYLOAD_SIZE - 3, file);
    }

    fclose(file);

    size = buildControlPacket(packet, 3, fileSize, filename);
    if (llSend(packet, size) < 0)
    {
        printf("Error sending END packet\n");
        return;
    }

    llCloseTx();
}

void receiveFile(const char *filename)
{
    unsigned char packet[MAX_PAYLOAD_SIZE];
    unsigned char startPacket[MAX_PAYLOAD_SIZE];
    int startSize = 0;
    long fileSize = 0;
    long received = 0;
    FILE *file = NULL;

    while (TRUE)
    {
        int size = llReceive(packet);
        if (size < 0)
        {
            printf("Error receiving packet\n");
            break;
        }

        if (packet[0] == 1)
        {
            fileSize = (packet[3] << 24) | (packet[4] << 16) | (packet[5] << 8) | packet[6];
            printf("START received: %.*s, %ld bytes\n", packet[8], &packet[9], fileSize);

            memcpy(startPacket, packet, size);
            startSize = size;

            file = fopen(filename, "wb");
            if (file == NULL)
            {
                perror("fopen");
                return;
            }
        }
        else if (packet[0] == 2 && file != NULL)
        {
            int k = 256 * packet[1] + packet[2];
            if (k != size - 3)
            {
                printf("Invalid data packet size\n");
            }
            else
            {
                fwrite(&packet[3], 1, k, file);
                received += k;
            }
        }
        else if (packet[0] == 3)
        {
            printf("END received\n");

            if (size != startSize || memcmp(&packet[1], &startPacket[1], size - 1) != 0)
            {
                printf("END packet does not match START packet\n");
            }
            if (received != fileSize)
            {
                printf("Received %ld bytes, expected %ld\n", received, fileSize);
            }
            break;
        }
    }

    if (file != NULL)
    {
        fclose(file);
    }

    llCloseRx();
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    // ----------------------------------------------------
    // TODO: Adapt and extend this code according to the specifications of the project.
    // ----------------------------------------------------

    LinkLayer llParameters = {
        .baudRate = baudRate,
        .nRetransmissions = nTries,
        .timeout = timeout,
    };
    strcpy(llParameters.serialPort, serialPort);

    if (strcmp(role, "tx") == 0)
    {
        if (llOpenTx(llParameters) < 0)
        {
            printf("Error opening connection\n");
            return;
        }
        sendFile(filename);
    }
    else if (strcmp(role, "rx") == 0)
    {
        if (llOpenRx(llParameters) < 0)
        {
            printf("Error opening connection\n");
            return;
        }
        receiveFile(filename);
    }
    else
    {
        printf("Invalid role: %s. Must be 'tx' or 'rx'.\n", role);
        return;
    }
}
