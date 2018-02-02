#ifndef ASSERTIONS_H
#define ASSERTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libxml/tree.h>
    
typedef struct {
    xmlNode** inputTpNodes;
    int nInputs;
    xmlNode** nodes;
    int n;
} NodeIdMap;
    
typedef struct {
    char* tpName;
    int valveNo;
    int isIndex;
    float min, max;
    int* truth;
} TestPoint;
    
typedef struct {
    TestPoint** tps;
    int nTp;
    int nInputs;
} AssertionsSet;

typedef struct LinkedListSortingNode LinkedListSortingNode;

struct LinkedListSortingNode {
    TestPoint* tp;
    int depth;
    LinkedListSortingNode* prev;
    LinkedListSortingNode* next;
};

int getIndexOfTPByName(AssertionsSet* set, const char* name);
int getIndexOfTPNodeInSet(AssertionsSet* set, xmlNode* node);
AssertionsSet* createAssertionSetFromXMLNode(xmlNode* circuitNode);
void freeAssertionSet(AssertionsSet* set);
void checkTruthTable(AssertionsSet* set, int* samples, int* dest, int* n);
void printTruthTable(AssertionsSet* set);
void printTPs(AssertionsSet* set);

#ifdef __cplusplus
}
#endif

#endif /* ASSERTIONS_H */

