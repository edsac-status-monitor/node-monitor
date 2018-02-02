#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <wiringPiSPI.h>
#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#include "assertions.h"
#include "circuit.h"
#include "resistors.h"
#include "xmlutil.h"
#include "tables.h"
    
#define NODE_NAME_TP "tp"
#define ATTR_NAME_ID "id"
#define ATTR_NAME_THRESHOLD "threshold"

#define TABLE_TITLE "Callibration"
#define TABLE_TP_HEADING "TP"
#define TABLE_THRESHOLD_HEADING "Threshold"

void setupResistors(int channel, int speed) {
    int code;
    code = wiringPiSPISetup(channel, speed);
    if(code < 0) {
        fprintf(stderr, "Error with wiringPiSPISetip(%d)\n", code);
    }
}

void teardownResistors() {
    
}

Threshold* createThreshold(int wiringIndex, float value) {
    Threshold* t;
    assert((t = malloc(sizeof(Threshold))) != NULL);
    t->wireIndex = wiringIndex;
    t->value = value;
    return t;
}

void freeThreshold(Threshold* t) {
    free(t);
}

Calibration* createCalibrationFromXMLNode(AssertionsSet* set, Wiring* wiring, xmlNode* calibrateNode) {
    assert(set != NULL);
    assert(wiring != NULL);
    assert(calibrateNode != NULL);
    Calibration* calibration;
    xmlNode* child;
    int j, tpIndex, wiringIndex;
    float thresholdValue;
    
    child = calibrateNode->children;
    calibration = malloc(sizeof(Calibration));
    calibration->thresholds = NULL;
    calibration->nThresholds = 0;
    while(child != NULL) {
        if(child->type == XML_ELEMENT_NODE) {
            if(strEqual(child->name, NODE_NAME_TP)) {
                if(!xmlHasProp(child, ATTR_NAME_ID)) {
                    freeCalibration(calibration);
                    fprintf(stderr, "TP node has no id\n");
                    return NULL;
                }
                if(!xmlHasProp(child, ATTR_NAME_THRESHOLD)) {
                    freeCalibration(calibration);
                    fprintf(stderr, "TP node has no threshold\n");
                    return NULL;
                }
                tpIndex = getIndexOfTPNodeInSet(set, child);
                if(tpIndex < 0) {
                    freeCalibration(calibration);
                    fprintf(stderr, "TP node refers to a tp not in the assertions set\n");
                    return NULL;
                }
                wiringIndex = getIndexOfTPIndexInWiring(wiring, tpIndex);
                if(wiringIndex < 0) {
                    freeCalibration(calibration);
                    fprintf(stderr, "TP node refers to a tp not in the wiring set\n");
                    return NULL;
                }
                
                for(j = 0; j < calibration->nThresholds; j++) {
                    if(calibration->thresholds[j]->wireIndex == wiringIndex) {
                        freeCalibration(calibration);
                        fprintf(stderr, "TP node has an invalid pin attribute\n");
                        return NULL;
                    }
                }
                thresholdValue = nodePropAsFloat(child, ATTR_NAME_THRESHOLD);
                if(thresholdValue < set->tps[tpIndex]->min || 
                        thresholdValue > set->tps[tpIndex]->max) {
                    
                    freeCalibration(calibration);
                    fprintf(stderr, "TP node has an invalid threshold attribute\n");
                    return NULL;
                }
                
                calibration->thresholds = realloc(calibration->thresholds, sizeof(Threshold*) * (calibration->nThresholds + 1));
                calibration->thresholds[calibration->nThresholds] = createThreshold(wiringIndex, thresholdValue);
                calibration->nThresholds++;
            } else {
                freeCalibration(calibration);
                fprintf(stderr, "Unknown node name: \"%s\"\n", child->name);
                return NULL;
            }
        }
        child = child->next;
    }
    
    if(calibration->nThresholds != set->nTp) {
        fprintf(stderr, "Number of TPs in thresholds (%d) does not match number of TPs in assertions (%d)\n", calibration->nThresholds, set->nTp);
        freeCalibration(calibration);
        return NULL;
    }
    
    return calibration;
}

void freeCalibration(Calibration* calibration) {
    int i;
    if(calibration != NULL) {
        for(i = 0; i < calibration->nThresholds; i++) {
            freeThreshold(calibration->thresholds[i]);
        }
        free(calibration);
    }
}

void printCalibration(AssertionsSet* set, Wiring* wiring, Calibration* calibration) {
    int i, j, maxCellStringLen, nColumns, nRows;
    char** columns;
    char*** rows;
    assert(set != NULL);
    assert(wiring != NULL);
    
    maxCellStringLen = 255;
    nColumns = 2;
    assert((columns = malloc(sizeof(char*) * nColumns)) != NULL);
    columns[0] = TABLE_TP_HEADING;
    columns[1] = TABLE_THRESHOLD_HEADING;
    nRows = calibration->nThresholds;
    assert((rows = malloc(sizeof(char**) * nRows)) != NULL);
    for(i = 0; i < nRows; i++) {
        rows[i] = malloc(sizeof(char*) * nColumns);
        rows[i][0] = malloc(sizeof(char) * maxCellStringLen);
        snprintf(rows[i][0], maxCellStringLen, "%s", set->tps[wiring->wires[calibration->thresholds[i]->wireIndex]->tpIndex]->tpName);
        rows[i][1] = malloc(sizeof(char) * maxCellStringLen);
        snprintf(rows[i][1], maxCellStringLen, "%f", calibration->thresholds[i]->value);
    }
    printTable(stdout, TABLE_TITLE, columns, nColumns, rows, nRows);
    for(i = 0; i < nRows; i++) {
        for(j = 0; j < nColumns; j++) {
            free(rows[i][j]);
        }
        free(rows[i]);
    }
    free(rows);
    free(columns);
}

uint8_t getResistanceValue(Wiring* wiring, int wiringIndex, float value) {
    float attenuation = wiring->wires[wiringIndex]->attenuation;
    return (uint8_t) roundf((1 - ((value / attenuation) / 5.0)) * 255.0);
}

void writeOutCallibrationStream(int spiChannel, Wiring* wiring, Calibration* calibration, int stream) {
    assert(wiring != NULL);
    assert(calibration != NULL);
    uint8_t* streamData;
    uint8_t commandByte;
    int i, wiringIndex, indexFromEnd;
    
    assert((streamData = malloc(sizeof(uint8_t) * wiring->nResistorChips * 2)) != NULL);
    for(i = 0; i < wiring->nResistorChips; i++) {
        streamData[(i*2)] = 0;
        streamData[(i*2)+1] = 0;
    }
    commandByte = 0x10 | stream;
    for(i = 0; i < calibration->nThresholds; i++) {
        wiringIndex = calibration->thresholds[i]->wireIndex;
        if(wiring->wires[wiringIndex]->resistor.resistor == stream) {
            indexFromEnd = wiring->nResistorChips - 1 - wiring->wires[wiringIndex]->resistor.chip;
            streamData[(indexFromEnd*2)] = commandByte;
            streamData[(indexFromEnd*2)+1] = getResistanceValue(wiring, wiringIndex, calibration->thresholds[i]->value);
        }
    }
    wiringPiSPIDataRW(spiChannel, streamData, wiring->nResistorChips * 2);
    free(streamData);
}

void writeOutCalibration(int spiChannel, Wiring* wiring, Calibration* calibration) {
    writeOutCallibrationStream(spiChannel, wiring, calibration, STREAM_A);
    writeOutCallibrationStream(spiChannel, wiring, calibration, STREAM_B);
}

int setThresholdForTPIndex(Wiring* wiring, Calibration* calibration, int tpIndex, float threshold) {
    int wiringIndex, i;
    wiringIndex = getIndexOfTPIndexInWiring(wiring, tpIndex);
    if(wiringIndex < 0) {
        fprintf(stderr, "TP index not contained in wiring\n");
        return -1;
    }
    for(i = 0; i < calibration->nThresholds; i++) {
        if(calibration->thresholds[i]->wireIndex == wiringIndex) {
            calibration->thresholds[i]->value = threshold;
            return 1;
        }
    }
    fprintf(stderr, "TP index not contained in calibration\n");
    return -1;
}

int setAndWriteThresholdForTPIndex(int spiChannel, Wiring* wiring, Calibration* calibration, int tpIndex, float threshold) {
    int wiringIndex;
    if(!setThresholdForTPIndex(wiring, calibration, tpIndex, threshold)) {
        return -1;
    }
    wiringIndex = getIndexOfTPIndexInWiring(wiring, tpIndex);
    writeOutCallibrationStream(spiChannel, wiring, calibration, wiring->wires[wiringIndex]->resistor.resistor);
    return 1;
}