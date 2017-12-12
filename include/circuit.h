#ifndef CIRCUIT_H
#define CIRCUIT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <libxml/tree.h>
#include "assertions.h"

#define STREAM_A 1
#define STREAM_B 2
    
typedef struct {
    int chip;
    int resistor;
} ResitorLocation;
typedef struct {
    int tpIndex;
    int gpioPin;
    float attenuation;
    ResitorLocation resistor;
} Wire;
typedef struct {
    Wire** wires;
    int nWires;
    int nResistorChips;
    int holdGpioPin;
} Wiring;
    
void setupWiring();
void teardownWiring();
int getIndexOfTPIndexInWiring(Wiring* wiring, int tpIndex);
Wiring* createWiringFromXMLNode(AssertionsSet* assertionsSet, xmlNode* wiringNode);
void freeWiring(Wiring* wiring);
void readInTPValues(Wiring* wiring, int* dest);
void printWiring(AssertionsSet* assertionsSet, Wiring* wiring);

#ifdef __cplusplus
}
#endif

#endif /* CIRCUIT_H */

