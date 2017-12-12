#ifndef RESISTORS_H
#define RESISTORS_H

#ifdef __cplusplus
extern "C" {
#endif
    
#include <stdint.h>
#include <libxml/tree.h>
    
typedef struct {
    int wireIndex;
    float value;
} Threshold;

typedef struct {
    Threshold** thresholds;
    int nThresholds;
} Calibration;
    
void setupResistors(int channel, int speed);
void teardownResistors();
Calibration* createCalibrationFromXMLNode(AssertionsSet* set, Wiring* wiring, xmlNode* calibrateNode);
void freeCalibration(Calibration* callibration);
void printCalibration(AssertionsSet* set, Wiring* wiring, Calibration* calibration);
void writeOutCalibration(int spiChannel, Wiring* wiring, Calibration* calibration);
int setThresholdForTPIndex(Wiring* wiring, Calibration* calibration, int tpIndex, float threshold);
int setAndWriteThresholdForTPIndex(int spiChannel, Wiring* wiring, Calibration* calibration, int tpIndex, float threshold);

#ifdef __cplusplus
}
#endif

#endif /* RESISTORS_H */

