#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <wiringPi.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "network.h"
#include "assertions.h"
#include "circuit.h"
#include "resistors.h"

#ifndef CIRCUIT_FILNAME
#define CIRCUIT_FILNAME "config/circuit.xml"
#endif
#ifndef WIRING_FILNAME
#define WIRING_FILNAME "config/wiring.xml"
#endif
#ifndef CALLIBRATE_FILNAME
#define CALLIBRATE_FILNAME "config/calibrate.xml"
#endif

#define SPI_CHANNEL 0
#define SPI_SPEED 50000

void parseCircuitFile(const char* filename, AssertionsSet** set) {
    xmlDoc *doc = NULL;
    xmlNode *root = NULL;
    doc = xmlReadFile(filename, NULL, 0);
    if(doc == NULL) {
        fprintf(stderr, "Failed to parse %s\n", filename);
        return;
    }
    root = xmlDocGetRootElement(doc);
    *set = createAssertionSetFromXMLNode(root);
    
    xmlFreeDoc(doc);
}

void parseWiringFile(const char* filename, AssertionsSet* set, Wiring** wiring) {
    xmlDoc *doc = NULL;
    xmlNode *root = NULL;
    doc = xmlReadFile(filename, NULL, 0);
    if(doc == NULL) {
        fprintf(stderr, "Failed to parse %s\n", filename);
        return;
    }
    root = xmlDocGetRootElement(doc);
    *wiring = createWiringFromXMLNode(set, root);
    
    xmlFreeDoc(doc);
}

void parseCalibrateFile(const char* filename, AssertionsSet* set, Wiring* wiring, Calibration** calibration) {
    xmlDoc *doc = NULL;
    xmlNode *root = NULL;
    doc = xmlReadFile(filename, NULL, 0);
    if(doc == NULL) {
        fprintf(stderr, "Failed to parse %s\n", filename);
        return;
    }
    root = xmlDocGetRootElement(doc);
    *calibration = createCalibrationFromXMLNode(set, wiring, root);
    
    xmlFreeDoc(doc);
}

void get(AssertionsSet** set, Wiring** wiring, Calibration** calibration) {
    parseCircuitFile(CIRCUIT_FILNAME, set);
    assert(*set != NULL);
    parseWiringFile(WIRING_FILNAME, *set, wiring);
    assert(*wiring != NULL);
    parseCalibrateFile(CALLIBRATE_FILNAME, *set, *wiring, calibration);
    assert(*calibration != NULL);
}

/*
 * 
 */
int main(int argc, char** argv) {
    LIBXML_TEST_VERSION

    NetworkHandle* netHndl;
    AssertionsSet* assertions;
    Wiring* wiring;
    Calibration* calibration;
    int* errorIndicesStore;
    int nErrors, i, j, valveNo;
    float f;
    
    setupWiring();
    setupResistors(SPI_CHANNEL, SPI_SPEED);
    /*netHndl = setupNetwork("127.0.0.1", 2000);
    if(netHndl == NULL) {
        return -1;
    }*/
    
    get(&assertions, &wiring, &calibration);
    printTPs(assertions);
    printTruthTable(assertions);
    printWiring(assertions, wiring);
    printCalibration(assertions, wiring, calibration);
    
    /*while(1) {
        for(f = 0; f < 5; f += (5.0 / 255)) {
            for(i = 0; i < 8; i++) {
                setThresholdForTPIndex(wiring, calibration, i, f);
            }
            writeOutCalibration(SPI_CHANNEL, wiring, calibration);
        }
    }*/
    //return EXIT_SUCCESS;
    
    writeOutCalibration(SPI_CHANNEL, wiring, calibration);
    
    char* tmpMsg = malloc(sizeof(char) * MAX_MSG_STR_LENGTH);
    errorIndicesStore = malloc((assertions->nTp - assertions->nInputs) * sizeof(int));
    
    int* tpValues = malloc(sizeof(int) * assertions->nTp);
    int* lastTPValues = malloc(sizeof(int) * assertions->nTp);
    int* tmpTPValues;
    for(;;) {
        readInTPValues(wiring, tpValues);
        for(i = 0; i < assertions->nTp; i++) {
            if(tpValues[i] != lastTPValues[i]) {
                checkTruthTable(assertions, tpValues, errorIndicesStore, &nErrors);
                if(nErrors > 0) {
                    printf("Data:");
                    for(j = 0; j < assertions->nTp; j++) {
                        printf(" %d", tpValues[j]);
                    }
                    printf("\n%d errors:\n", nErrors);
                    for(j = 0; j < nErrors; j++) {
                        valveNo = assertions->tps[errorIndicesStore[j]]->valveNo;
                        snprintf(tmpMsg, MAX_MSG_STR_LENGTH, "Valve %d failed, registered on tp %s", valveNo, assertions->tps[errorIndicesStore[j]]->tpName);
                        printf("Error[%d] %s\n", j, tmpMsg);
                        //sendNetworkMessage(netHndl, valveNo, tmpMsg);
                    }
                }
                break;
            }
        }
        tmpTPValues = tpValues;
        tpValues = lastTPValues;
        lastTPValues = tmpTPValues;
    }
    
    free(errorIndicesStore);
    free(tmpMsg);
    
    freeAssertionSet(assertions);
    freeWiring(wiring);
    freeCalibration(calibration);
    
    teardownResistors();
    teardownWiring();
    //teardownNetwork(netHndl);
    
    return (EXIT_SUCCESS);
}

