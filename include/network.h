#ifndef NETWORK_H
#define NETWORK_H

#ifdef __cplusplus
extern "C" {
#endif
    
#include "edsac_representation.h"

#define MAX_MSG_STR_LENGTH 200

typedef struct {
    Message* msgStruct;
} NetworkHandle;

NetworkHandle* setupNetwork(const char* addrStr, int port);
int sendNetworkMessage(NetworkHandle* network, int valveNo, char* msg);
void teardownNetwork(NetworkHandle* network);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_H */

