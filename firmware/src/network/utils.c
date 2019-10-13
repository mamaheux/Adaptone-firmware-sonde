#include "network/utils.h"

#include <lwip/sockets.h>

void freeSocket(int socketHandle)
{
    shutdown(socketHandle, 0);
    close(socketHandle);
}
