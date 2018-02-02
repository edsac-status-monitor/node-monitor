#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "network.h"
#include "edsac_representation.h"
#include "edsac_sending.h"
#include "edsac_arguments.h"

NetworkHandle* setupNetwork(const char* addrStr, int port) {
    assert(MAX_MSG_STR_LENGTH <= MAX_MSG_LEN);
    bool state;
    struct sockaddr* adr = alloc_addr(addrStr, port);
    assert(NULL != adr);
    state = start_sending(adr, sizeof(*adr));
    free(adr);
    if(state != true) {
        fprintf(stderr, "Could not start sending\n");
        return NULL;
    }
    NetworkHandle* network = malloc(sizeof(NetworkHandle));
    network->msgStruct = malloc(sizeof(Message));
    return network;
}

int sendNetworkMessage(NetworkHandle* network, int valveNo, char* msg) {
    bool state;
    hardware_error_valve(network->msgStruct, valveNo, msg);
    state = send_message(network->msgStruct);
    free_message(network->msgStruct);
    if(state != true) {
        fprintf(stderr, "Could not send message\n");
        return -1;
    }
    return 1;
}

void teardownNetwork(NetworkHandle* network) {
    free(network->msgStruct);
    free(network);
    
    stop_sending();
}

