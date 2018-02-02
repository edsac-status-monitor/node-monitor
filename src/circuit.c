#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include "circuit.h"
#include "xmlutil.h"
#include "tables.h"
    
#define NODE_NAME_TP "tp"
#define ATTR_NAME_HOLD_PIN "hold-pin"
#define ATTR_NAME_ID "id"
#define ATTR_NAME_PIN "pin"
#define ATTR_NAME_ATTENUATION "attenuation"
#define ATTR_NAME_RESISTOR "resistor"

#define TABLE_TITLE "Wiring"
#define TABLE_TP_HEADING "TP"
#define TABLE_PIN_HEADING "BCM Pin"
#define TABLE_RESISTOR_HEADING "Resistor"

void setupWiring() {
    wiringPiSetupGpio();
}

void teardownWiring() {
    
}

Wire* createWire(int tpIndex, int pin, float attenuation, int resistorChip, int resistorOnChip) {
    Wire* wire = malloc(sizeof(Wire));
    wire->tpIndex = tpIndex;
    wire->gpioPin = pin;
    wire->attenuation = attenuation;
    wire->resistor.chip = resistorChip;
    wire->resistor.resistor = resistorOnChip;
    return wire;
}

void freeWire(Wire* wire) {
    free(wire);
}

int getIndexOfTPIndexInWiring(Wiring* wiring, int tpIndex) {
    int i;
    for(i = 0; i < wiring->nWires; i++) {
        if(wiring->wires[i]->tpIndex == tpIndex) {
            return i;
        }
    }
    return -1;
}

int parseGpioPinAttr(xmlNode* node, const char* attrName) {
    int pin;
    assert(node != NULL);
    assert(attrName != NULL);
    pin = nodePropAsInteger(node, attrName);
    if(pin <= 0) {
        fprintf(stderr, "%s node has an invalid %s attribute\n", node->name, attrName);
        return -1;
    }
    pin = physPinToGpio(pin);
    if(pin < 0) {
        fprintf(stderr, "TP node pin attribute refers to an invalid pin\n");
        return -1;
    }
    return pin;
}

Wiring* createWiringFromXMLNode(AssertionsSet* set, xmlNode* wiringNode) {
    assert(set != NULL);
    assert(wiringNode != NULL);
    int j, tpIndex, pin, resistorChipIndex, resistorOnChip;
    float attenuation;
    char resistorOnChipChar;
    Wiring* wiring;
    xmlNode* child;
    
    wiring = malloc(sizeof(Wiring));
    wiring->wires = NULL;
    wiring->nWires = 0;
    wiring->nResistorChips = 0;
    
    if(!xmlHasProp(wiringNode, ATTR_NAME_HOLD_PIN)) {
        fprintf(stderr, "Wiring node has no hold pin attribute\n");
        freeWiring(wiring);
        return NULL;
    }
    pin = parseGpioPinAttr(wiringNode, ATTR_NAME_HOLD_PIN);
    if(pin < 0) {
        freeWiring(wiring);
        return NULL;
    }
    wiring->holdGpioPin = pin;
    
    child = wiringNode->children;
    while(child != NULL) {
        if(child->type == XML_ELEMENT_NODE) {
            if(strEqual(child->name, NODE_NAME_TP)) {
                if(!xmlHasProp(child, ATTR_NAME_ID)) {
                    freeWiring(wiring);
                    fprintf(stderr, "TP node has no id\n");
                    return NULL;
                }
                if(!xmlHasProp(child, ATTR_NAME_PIN)) {
                    freeWiring(wiring);
                    fprintf(stderr, "TP node has no pin\n");
                    return NULL;
                }
                if(!xmlHasProp(child, ATTR_NAME_RESISTOR)) {
                    freeWiring(wiring);
                    fprintf(stderr, "TP node has no resistor\n");
                    return NULL;
                }
                tpIndex = getIndexOfTPNodeInSet(set, child);
                if(tpIndex < 0) {
                    freeWiring(wiring);
                    fprintf(stderr, "TP node refers to a tp not in the assertions set\n");
                    return NULL;
                }
                for(j = 0; j < wiring->nWires; j++) {
                    if(wiring->wires[j]->tpIndex == tpIndex) {
                        freeWiring(wiring);
                        fprintf(stderr, "TP node has an invalid pin attribute\n");
                        return NULL;
                    }
                }
                pin = parseGpioPinAttr(child, ATTR_NAME_PIN);
                if(pin < 0) {
                    freeWiring(wiring);
                    return NULL;
                }
                attenuation = 1.0;
                if(xmlHasProp(child, ATTR_NAME_ATTENUATION)) {
                    attenuation = nodePropAsFloat(child, ATTR_NAME_ATTENUATION);
                    if(attenuation == 0 || attenuation < 0) {
                        freeWiring(wiring);
                        fprintf(stderr, "TP node attenuation attribute is invalid\n");
                        return NULL;
                    }
                }
                j = nodePropScanf(child, ATTR_NAME_RESISTOR, "%d%c", &resistorChipIndex, &resistorOnChipChar);
                if(j != 2) {
                    freeWiring(wiring);
                    fprintf(stderr, "TP node resistor attribute could not be parsed\n");
                    return NULL;
                }
                if(resistorOnChipChar == 'A' || resistorOnChipChar == 'a') {
                    resistorOnChip = STREAM_A;
                } else {
                    resistorOnChip = STREAM_B;
                }
                
                wiring->wires = realloc(wiring->wires, sizeof(Wire*) * (wiring->nWires + 1));
                wiring->wires[wiring->nWires] = createWire(tpIndex, pin, attenuation, resistorChipIndex, resistorOnChip);
                wiring->nWires++;
                if(resistorChipIndex + 1 > wiring->nResistorChips) {
                    wiring->nResistorChips = resistorChipIndex + 1;
                }
            } else {
                freeWiring(wiring);
                fprintf(stderr, "Unknown node name: \"%s\"\n", child->name);
                return NULL;
            }
        }
        child = child->next;
    }
    
    if(wiring->nWires != set->nTp) {
        freeWiring(wiring);
        fprintf(stderr, "Unknown node name: \"%s\"\n", child->name);
        return NULL;
    }
    
    pinMode(wiring->holdGpioPin, OUTPUT);
    digitalWrite(wiring->holdGpioPin, HIGH);
    for(j = 0; j < wiring->nWires; j++) {
        pinMode(wiring->wires[j]->gpioPin, INPUT);
    }
    
    return wiring;
}

void freeWiring(Wiring* wiring) {
    int i;
    if(wiring != NULL) {
        for(i = 0; i < wiring->nWires; i++) {
            freeWire(wiring->wires[i]);
        }
        free(wiring);
    }
}

void readInTPValues(Wiring* wiring, int* dest) {
    int i;
    digitalWrite(wiring->holdGpioPin, LOW);
    for(i = 0; i < wiring->nWires; i++) {
        dest[wiring->wires[i]->tpIndex] = digitalRead(wiring->wires[i]->gpioPin);
    }
    digitalWrite(wiring->holdGpioPin, HIGH);
}

void printWiring(AssertionsSet* set, Wiring* wiring) {
    int i, j, maxCellStringLen, nColumns, nRows;
    char** columns;
    char*** rows;
    assert(set != NULL);
    assert(wiring != NULL);
    
    maxCellStringLen = 255;
    nColumns = 3;
    assert((columns = malloc(sizeof(char*) * nColumns)) != NULL);
    columns[0] = TABLE_TP_HEADING;
    columns[1] = TABLE_PIN_HEADING;
    columns[2] = TABLE_RESISTOR_HEADING;
    nRows = wiring->nWires;
    assert((rows = malloc(sizeof(char**) * nRows)) != NULL);
    for(i = 0; i < nRows; i++) {
        rows[i] = malloc(sizeof(char*) * nColumns);
        rows[i][0] = malloc(sizeof(char) * maxCellStringLen);
        snprintf(rows[i][0], maxCellStringLen, "%s", set->tps[wiring->wires[i]->tpIndex]->tpName);
        rows[i][1] = malloc(sizeof(char) * maxCellStringLen);
        snprintf(rows[i][1], maxCellStringLen, "%d", wiring->wires[i]->gpioPin);
        rows[i][2] = malloc(sizeof(char) * maxCellStringLen);
        snprintf(rows[i][2], maxCellStringLen, "%d%c", wiring->wires[i]->resistor.chip, wiring->wires[i]->resistor.resistor == STREAM_A ? 'A' : 'B');
    }
    printf("Wiring. HOLD GPIO PIN: %d\n", wiring->holdGpioPin);
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