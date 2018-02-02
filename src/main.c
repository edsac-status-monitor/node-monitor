#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <wiringPi.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "assertions.h"
#include "circuit.h"
#include "network.h"
#include "resistors.h"
#include "samples.h"

#ifndef CIRCUIT_FILNAME
#define CIRCUIT_FILNAME "config/circuit.xml"
#endif
#ifndef WIRING_FILNAME
#define WIRING_FILNAME "config/wiring.xml"
#endif
#ifndef CALLIBRATE_FILNAME
#define CALLIBRATE_FILNAME "config/calibrate.xml"
#endif

#define PROGRAM_NAME "edsac_status_monitor"
#define MAX_ARG_LEN 128
#define N_PARAMS 10
#define CONFIG_DIR "config"
#define CHASSIS_FILE "circuit.xml"
#define WIRING_FILE "wiring.xml"
#define CALLIBRATION_FILE "calibrate.xml"
#define TX_ADDRESS "127.0.0.1"
#define TX_PORT 2000
#define ECHO_ONLY 0

#define FILE_SEPARATOR '/'
#define SPI_CHANNEL 0
#define SPI_SPEED 50000
    
typedef struct {
    const char* name;
    const char* format;
    void* dest;
    const char* argsName;
    const char* description;
} CmdLineParam;

typedef struct {
    char* configDirectory;
    char* circuitFile;
    char* wiringFile;
    char* calibrationFile;
    char* samplesFile;
    char* txAddr;
    int txPort;
    int echoOnly, readInOnly, helpMessage;
} CmdLineOptions;

CmdLineParam params[N_PARAMS] = {
    { .name="--config-dir", .format="%s", .dest=NULL, .argsName="<directory>", .description="The directory in which to look for configuration files"},
    { .name="--chassis-file", .format="%s", .dest=NULL, .argsName="<filename>", .description="The filename of the chassis configuration file within the configuration directory"},
    { .name="--wiring-file", .format="%s", .dest=NULL, .argsName="<filename>", .description="The filename of the wiring configuration file within the configuration directory"},
    { .name="--calibration-file", .format="%s", .dest=NULL, .argsName="<filename>", .description="The filename of the calibration configuration file within the configuration directory"},
    { .name="--tx-addr", .format="%s", .dest=NULL, .argsName="<address>", .description="The IP address of the mothership"},
    { .name="--tx-port", .format="%d", .dest=NULL, .argsName="<port>", .description="The IP port upon which to communicate with the mothership with"},
    { .name="--no-up-network", .format=NULL, .dest=NULL, .argsName=NULL, .description="Do not relay any error messages to the mothership and simply echo them"},
    { .name="--help", .format=NULL, .dest=NULL, .argsName=NULL, .description="Display this help message"},
    { .name="--read-config", .format=NULL, .dest=NULL, .argsName=NULL, .description="Echo the parsed contents of the configuration files"},
    { .name="--test-sample-file", .format="%s", .dest=NULL, .argsName="<filename>", .description="The filename of a csv file containing sample data to use instead of sampling from GPIO pins"}
};

AssertionsSet* parseCircuitFile(const char* filename) {
    AssertionsSet* set;
    xmlDoc *doc = NULL;
    xmlNode *root = NULL;
    
    if(access(filename, R_OK) != 0) {
        fprintf(stderr, "The expected chassis file \"%s\" does not exist or could not be read\n", filename);
        return NULL;
    }
    
    doc = xmlReadFile(filename, NULL, 0);
    if(doc == NULL) {
        fprintf(stderr, "Failed to parse \"%s\" as an XML document\n", filename);
    } else {
        root = xmlDocGetRootElement(doc);
        set = createAssertionSetFromXMLNode(root);
        xmlFreeDoc(doc);
    }
    
    return set;
}

Wiring* parseWiringFile(const char* filename, AssertionsSet* set) {
    Wiring* wiring = NULL;
    xmlDoc *doc = NULL;
    xmlNode *root = NULL;
    
    if(access(filename, R_OK) != 0) {
        fprintf(stderr, "The expected wiring file \"%s\" does not exist or could not be read\n", filename);
        return NULL;
    }
    
    doc = xmlReadFile(filename, NULL, 0);
    if(doc == NULL) {
        fprintf(stderr, "Failed to parse \"%s\" as an XML document\n", filename);
    } else {
        root = xmlDocGetRootElement(doc);
        wiring = createWiringFromXMLNode(set, root);
        xmlFreeDoc(doc);
    }
    
    return wiring;
}

Calibration* parseCalibrateFile(const char* filename, AssertionsSet* set, Wiring* wiring) {
    Calibration* calibration = NULL;
    xmlDoc *doc = NULL;
    xmlNode *root = NULL;
    
    if(access(filename, R_OK) != 0) {
        fprintf(stderr, "The expected calibration file \"%s\" does not exist or could not be read\n", filename);
        return NULL;
    }
    
    doc = xmlReadFile(filename, NULL, 0);
    if(doc == NULL) {
        fprintf(stderr, "Failed to parse \"%s\" as an XML document\n", filename);
    } else {
        root = xmlDocGetRootElement(doc);
        calibration = createCalibrationFromXMLNode(set, wiring, root);
        xmlFreeDoc(doc);
    }
    
    return calibration;
}

char* addressOfFileInDirectory(const char* dir, const char* file) {
    char* dst;
    int lenDir, lenFile;
    
    assert(dir != NULL);
    assert(file != NULL);
    
    lenDir = strlen(dir);
    lenFile = strlen(file);
    assert((dst = malloc(sizeof(char) * (lenDir + 1 + lenFile + 1))) != NULL);
    
    strcpy(dst, dir);
    *(dst + lenDir) = FILE_SEPARATOR;
    strcpy(dst + lenDir + 1, file);
    
    return dst;
}

int get(CmdLineOptions* options, AssertionsSet** set, Wiring** wiring, Calibration** calibration) {
    char* file;
    
    file = addressOfFileInDirectory(options->configDirectory, options->circuitFile);
    *set = parseCircuitFile(file);
    free(file);
    
    if(*set != NULL) {
        file = addressOfFileInDirectory(options->configDirectory, options->wiringFile);
        *wiring = parseWiringFile(file, *set);
        free(file);
        
        if(*wiring != NULL) {
            file = addressOfFileInDirectory(options->configDirectory, options->calibrationFile);
            *calibration = parseCalibrateFile(file, *set, *wiring);
            free(file);
            
            if(*calibration != NULL) {
                return 1;
            }
            freeWiring(*wiring);
        }
        freeAssertionSet(*set);
    }
    return 0;
}

void printHelp(int argc, char** argv) {
    int maxOptionLen, j;
    char* programName;
    
    programName = PROGRAM_NAME;
    if(argc > 0) {
        programName = argv[0];
    }
    
    printf("Usage: %s [options]\nOptions:\n", programName);
    maxOptionLen = 0;
    char** optionStrings;
    assert((optionStrings = malloc(sizeof(char*) * N_PARAMS)) != NULL);
    for(j = 0; j < N_PARAMS; j++) {
        int len = strlen(params[j].name);
        if(params[j].format != NULL) {
            len += 1 + strlen(params[j].argsName);
        }
        assert((optionStrings[j] = malloc(sizeof(char) * (len + 1))) != NULL);
        if(params[j].format != NULL) {
            snprintf(optionStrings[j], len + 1, "%s %s", params[j].name, params[j].argsName);
        } else {
            snprintf(optionStrings[j], len + 1, "%s", params[j].name);
        }
        if(len > maxOptionLen) {
            maxOptionLen = len;
        }
    }
    for(j = 0; j < N_PARAMS; j++) {
        printf("  %-*s %s\n", maxOptionLen + 1, optionStrings[j], params[j].description);
        free(optionStrings[j]);
    }
    free(optionStrings);
}

void freeOptions(CmdLineOptions* options) {
    assert(options != NULL);
    free(options->txAddr);
    free(options);
}

CmdLineOptions* parseCommandLine(int argc, char** argv) {
    CmdLineOptions* options;
    char* programName;
    int i, j, k, optionsParsingFailed;
    
    programName = PROGRAM_NAME;
    if(argc > 0) {
        programName = argv[0];
    }

    assert((options = malloc(sizeof(CmdLineOptions))) != NULL);
    //Config Directory
    options->configDirectory = malloc(sizeof(char) * (MAX_ARG_LEN + 1));
    assert(strlen(CONFIG_DIR) <= MAX_ARG_LEN);
    strcpy(options->configDirectory, CONFIG_DIR);
    //Chassis File
    options->circuitFile = malloc(sizeof(char) * (MAX_ARG_LEN + 1));
    assert(strlen(CHASSIS_FILE) <= MAX_ARG_LEN);
    strcpy(options->circuitFile, CHASSIS_FILE);
    //Wiring File
    options->wiringFile = malloc(sizeof(char) * (MAX_ARG_LEN + 1));
    assert(strlen(WIRING_FILE) <= MAX_ARG_LEN);
    strcpy(options->wiringFile, WIRING_FILE);
    //Calibration File
    options->calibrationFile = malloc(sizeof(char) * (MAX_ARG_LEN + 1));
    assert(strlen(CALLIBRATION_FILE) <= MAX_ARG_LEN);
    strcpy(options->calibrationFile, CALLIBRATION_FILE);
    //TX Addr File
    options->txAddr = malloc(sizeof(char) * (MAX_ARG_LEN + 1));
    assert(strlen(TX_ADDRESS) <= MAX_ARG_LEN);
    strcpy(options->txAddr, TX_ADDRESS);
    //TX Port
    options->txPort = TX_PORT;
    //No Network
    options->echoOnly = ECHO_ONLY;
    //No monitoring
    options->readInOnly = 0;
    //Help Message
    options->helpMessage = 0;
    //Samples File
    options->samplesFile = malloc(sizeof(char) * (MAX_ARG_LEN + 1));
    strcpy(options->samplesFile, "");
    
    params[0].dest = options->configDirectory;
    params[1].dest = options->circuitFile;
    params[2].dest = options->wiringFile;
    params[3].dest = options->calibrationFile;
    params[4].dest = options->txAddr;
    params[5].dest = &options->txPort;
    params[6].dest = &options->echoOnly;
    params[7].dest = &options->helpMessage;
    params[8].dest = &options->readInOnly;
    params[9].dest = options->samplesFile;
    
    optionsParsingFailed = 0;
    for(i = 1; i < argc && !optionsParsingFailed; i++) {
        for(j = 0; j < N_PARAMS; j++) {
            if(strcmp(argv[i], params[j].name) == 0) {
                if(params[j].format != NULL) {
                    i++;
                    if(i >= argc) {
                        fprintf(stderr, "No value specified for %s option\n", argv[i]);
                        optionsParsingFailed = 1;
                        break;
                    }
                    if(strlen(argv[i]) > MAX_ARG_LEN) {
                        fprintf(stderr, "Value specified for %s option, \"%s\", is too large. Maximum length is %d\n", params[j].name, argv[i], MAX_ARG_LEN);
                        optionsParsingFailed = 1;
                        break;
                    }
                    k = sscanf(argv[i], params[j].format, params[j].dest);
                    if(k != 1) {
                        fprintf(stderr, "Value specified for %s option, \"%s\", could not be parsed (%d)\n", params[j].name, argv[i], k);
                        optionsParsingFailed = 1;
                        break;
                    }
                } else {
                    *((int*)params[j].dest) = true;
                }
                break;
            }
        }
        if(j >= N_PARAMS) {
            fprintf(stderr, "Unrecognised option, \"%s\"\n", argv[i]);
            optionsParsingFailed = 1;
            break;
        }
    }
    
    if(optionsParsingFailed) {
        printf("Try \"%s --help\" for help on using this program\n", programName);
        return NULL;
    }
    
    return options;
}

int main(int argc, char** argv) {
    LIBXML_TEST_VERSION

    CmdLineOptions* options;
    NetworkHandle* netHndl;
    AssertionsSet* assertions;
    Wiring* wiring;
    Calibration* calibration;
    Samples* samples;
    int* errorIndicesStore;
    int nErrors, i, j, valveNo;
    float f;
    
    options = parseCommandLine(argc, argv);
    if(options == NULL) {
        return -1;
    }
    
    if(options->helpMessage) {
        printHelp(argc, argv);
    } else {
        // Parsing wiring files requires the wiringPi to be initialised to convert physical pins to BCM pins.
        setupWiring();
        
        i = get(options, &assertions, &wiring, &calibration);
        
        if(i == 0) {
            fprintf(stderr, "Configuration file parsing failed\n");
        } else {
            if(options->readInOnly) {
                printTPs(assertions);
                printTruthTable(assertions);
                printWiring(assertions, wiring);
                printCalibration(assertions, wiring, calibration);
            } else {
                if(!options->echoOnly) {
                    netHndl = setupNetwork(options->txAddr, options->txPort);
                    if(netHndl == NULL) {
                        fprintf(stderr, "Failed to open network sender on %s:%d\n", options->txAddr, options->txPort);
                        return -1;
                    }
                }

                if(strlen(options->samplesFile) == 0) {
                    setupResistors(SPI_CHANNEL, SPI_SPEED);
                    writeOutCalibration(SPI_CHANNEL, wiring, calibration);
                } else {
                    samples = createSamplesFromFile(assertions, options->samplesFile);
                }

                if(strlen(options->samplesFile) != 0 && samples == NULL) {
                    fprintf(stderr, "Samples file parsing failed\n");
                } else {
                    char* tmpMsg = malloc(sizeof(char) * MAX_MSG_STR_LENGTH);
                    errorIndicesStore = malloc((assertions->nTp - assertions->nInputs) * sizeof(int));

                    int* tpValues = malloc(sizeof(int) * assertions->nTp);
                    int* lastTPValues = malloc(sizeof(int) * assertions->nTp);
                    int* tmpTPValues;
                    for(;;) {
                        if(strlen(options->samplesFile) == 0) {
                            readInTPValues(wiring, tpValues);
                        } else {
                            if(!samplesNext(samples)) {
                                break;
                            }
                            samplesGetValues(wiring, samples, tpValues);
                        }
                        for(i = 0; i < assertions->nTp; i++) {
                            if(tpValues[i] != lastTPValues[i]) {
                                checkTruthTable(assertions, tpValues, errorIndicesStore, &nErrors);
                                if(nErrors > 0) {
                                    if(options->echoOnly) {
                                        printf("Data:");
                                        for(j = 0; j < assertions->nTp; j++) {
                                            printf(" %d", tpValues[j]);
                                        }
                                        printf("\n%d errors:\n", nErrors);
                                        for(j = 0; j < nErrors; j++) {
                                            valveNo = assertions->tps[errorIndicesStore[j]]->valveNo;
                                            snprintf(tmpMsg, MAX_MSG_STR_LENGTH, "Valve %d failed, registered on tp %s", valveNo, assertions->tps[errorIndicesStore[j]]->tpName);
                                            printf("Error[%d] %s\n", j, tmpMsg);
                                        }
                                    } else {
                                        for(j = 0; j < nErrors; j++) {
                                            sendNetworkMessage(netHndl, valveNo, tmpMsg);
                                        }
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
                }
                
                if(netHndl != NULL) {
                    teardownNetwork(netHndl);
                }
                
                if(strlen(options->samplesFile) == 0) {
                    teardownResistors();
                } else {
                    if(samples != NULL) {
                        freeSamples(samples);
                    }
                }
            }

            freeAssertionSet(assertions);
            freeWiring(wiring);
            freeCalibration(calibration);
        }
        
        teardownWiring();
    }
    
    freeOptions(options);
    
    return EXIT_SUCCESS;
}