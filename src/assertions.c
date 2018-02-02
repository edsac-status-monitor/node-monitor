#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#include "assertions.h"
#include "xmlutil.h"
#include "tables.h"
    
#define NODE_NAME_CIRCUIT "circuit"
#define NODE_NAME_TP "tp"
#define NODE_NAME_NODE "node"
#define NODE_NAME_REF "ref"
#define NODE_NAME_AND "and"
#define NODE_NAME_OR "or"
#define NODE_NAME_NOT "not"
#define ATTR_NAME_ID "id"
#define ATTR_NAME_MIN "min"
#define ATTR_NAME_MAX "max"
#define ATTR_NAME_VALVE_NO "valve_no"
#define OP_AND 1
#define OP_OR 2
#define OP_NOT 3

#define YES_STR "Yes"
#define NO_STR "No"
#define TRUTH_TABLE_TITLE "Truth Table"
#define TP_TABLE_TITLE "Test Points"
#define TP_TABLE_HEADER_TP "TP";
#define TP_TABLE_HEADER_IS_INPUT "Input";
#define TP_TABLE_HEADER_VALVE_NO "Valve No.";
#define TP_TABLE_HEADER_MIN_V "Minimum (V)";
#define TP_TABLE_HEADER_MAX_V "Maximum (V)";

NodeIdMap* createNodeIdMap() {
    NodeIdMap* map = malloc(sizeof(NodeIdMap));
    map->inputTpNodes = NULL;
    map->nInputs = 0;
    map->nodes = NULL;
    map->n;
    return map;
};

void addNodeToIdMap(NodeIdMap* map, xmlNode* node) {
    map->nodes = realloc(map->nodes, (map->n + 1) * sizeof(xmlNode*));
    map->nodes[map->n] = node;
    map->n++;
}

xmlNode* findNodeInMap(NodeIdMap* map, char* id) {
    int i;
    for(i = 0; i < map->n; i++) {
        if(nodePropEqual(map->nodes[i], ATTR_NAME_ID, id)) {
            return map->nodes[i];
        }
    }
    fprintf(stderr, "No such node id found \"%s\"\n", id);
    return NULL;
}

void addInputNodeToIdMap(NodeIdMap* map, xmlNode* node) {
    map->inputTpNodes = realloc(map->inputTpNodes, (map->nInputs + 1) * sizeof(char*));
    map->inputTpNodes[map->nInputs] = node;
    map->nInputs++;
}

int findInputIndexInMap(NodeIdMap* map, xmlNode* node) {
    int i;
    for(i = 0; i < map->nInputs; i++) {
        if(map->inputTpNodes[i] == node) {
            return i;
        }
    }
    fprintf(stderr, "No such input found\n");
    return -1;
}

void freeNodeIdMap(NodeIdMap* map) {
    if(map != NULL) {
        free(map->inputTpNodes);
        free(map->nodes);
        free(map);
    }
}

int getIndexOfTPByName(AssertionsSet* set, const char* name) {
    int i;
    for(i = 0; i < set->nTp; i++) {
        if(strcmp(set->tps[i]->tpName, name) == 0) {
            return i;
        }
    }
    return -1;
}

int getIndexOfTPNodeInSet(AssertionsSet* set, xmlNode* node) {
    int i;
    for(i = 0; i < set->nTp; i++) {
        if(nodePropEqual(node, ATTR_NAME_ID, set->tps[i]->tpName)) {
            return i;
        }
    }
    return -1;
}

int* parseNode(xmlNode* node, NodeIdMap* map, int* depthOut, int* valveNoOut) {
    xmlNode* child = node->children;
    xmlChar* contents = NULL;
    int i, op = -1, inputIndex;
    int* truth = NULL; 
    int* truth2 = NULL;
    int depth1, depth2;
    if(strEqual(node->name, NODE_NAME_REF)) {
        contents = xmlNodeListGetString(node->doc, node->children, 1);
        truth = parseNode(findNodeInMap(map, contents), map, depthOut, valveNoOut);
        assert(truth != NULL);
        xmlFree(contents);
        return truth;
    } else if(strEqual(node->name, NODE_NAME_AND) || strEqual(node->name, NODE_NAME_OR) || 
            strEqual(node->name, NODE_NAME_NOT)) {
        
        if(strEqual(node->name, NODE_NAME_AND)) {
            op = OP_AND;
        } else if(strEqual(node->name, NODE_NAME_OR)) {
            op = OP_OR;
        } else if(strEqual(node->name, NODE_NAME_NOT)) {
            op = OP_NOT;
        }
        if(!xmlHasProp(node, ATTR_NAME_VALVE_NO)) {
            fprintf(stderr, "%s node has no valveNo\n", node->name);
            return NULL;
        }
        while(child != NULL && child->type != XML_ELEMENT_NODE) {
            child = child->next;
        }
        if(child) {
            truth = parseNode(child, map, &depth1, NULL);
            assert(truth != NULL);
            if(op == OP_NOT) {
                for(i = 0; i < (1 << map->nInputs); i++) {
                    truth[i] = !truth[i];
                }
            }
            child = child->next;
            while(child) {
                if(child->type == XML_ELEMENT_NODE) {
                    truth2 = parseNode(child, map, &depth2, NULL);
                    assert(truth2 != NULL);
                    depth1 = fmax(depth1, depth2);
                    for(i = 0; i < (1 << map->nInputs); i++) {
                        if(op == OP_AND) {
                            truth[i] = truth[i] && truth2[i];
                        } else if(op == OP_OR) {
                            truth[i] = truth[i] || truth2[i];
                        } else if(op == OP_NOT) {
                            fprintf(stderr, "Not operator can only have one param\n");
                            return NULL;
                        }
                    }
                    free(truth2);
                }
                child = child->next;
            }
            if(valveNoOut != NULL) {
                contents = xmlGetProp(node, ATTR_NAME_VALVE_NO);
                *valveNoOut = atoi(contents);
                xmlFree(contents);
            }
            *depthOut = depth1 + 1;
            return truth;
        } else {
            fprintf(stderr, "Operator node has no params\n");
            return NULL;
        }
    } else if(strEqual(node->name, NODE_NAME_TP)) {
        while(child) {
            if(child->type == XML_ELEMENT_NODE) {
                truth = parseNode(child, map, depthOut, valveNoOut);
                assert(truth != NULL);
                return truth;
            }
            child = child->next;
        }
        inputIndex = findInputIndexInMap(map, node);
        if(inputIndex < 0) {
            fprintf(stderr, "TP has no child nodes but isn't an indexed input\n");
            return NULL;
        }
        truth = malloc((1 << map->nInputs) * sizeof(int));
        for(i = 0; i < (1 << map->nInputs); i++) {
            truth[i] = (i & (1 << inputIndex)) != 0;
        }
        *depthOut = 0;
        if(valveNoOut != NULL) {
            *valveNoOut = -1;
        }
        return truth;
    } else {
        fprintf(stderr, "Unknown node type \"%s\"\n", node->name);
        return NULL;
    }
}

TestPoint* createTestPointFromXMLNode(xmlNode* tpNode, int* truthTable, int valveNo) {
    TestPoint* tp;
    xmlChar* tempStr;
    if(!xmlHasProp(tpNode, ATTR_NAME_ID)) {
        fprintf(stderr, "TP Node has no id");
        return NULL;
    }
    if(!xmlHasProp(tpNode, ATTR_NAME_MIN)) {
        fprintf(stderr, "TP Node has no minimum value");
        return NULL;
    }
    if(!xmlHasProp(tpNode, ATTR_NAME_MAX)) {
        fprintf(stderr, "TP Node has no maximum value");
        return NULL;
    }
    tp = malloc(sizeof(TestPoint));
    tp->valveNo = valveNo;
    tp->truth = truthTable;
    tempStr = xmlGetProp(tpNode, ATTR_NAME_ID);
    tp->tpName = strcpy(malloc((xmlStrlen(tempStr)+1) * sizeof(char)), tempStr);
    free(tempStr);
    tp->min = nodePropAsInteger(tpNode, ATTR_NAME_MIN);
    tp->max = nodePropAsInteger(tpNode, ATTR_NAME_MAX);
    return tp;
}

void freeTestPoint(TestPoint* tp) {
    if(tp != NULL) {
        free(tp->tpName);
        free(tp->truth);
        free(tp);
    }
}

AssertionsSet* createAssertionSetFromXMLNode(xmlNode* circuitNode) {
    AssertionsSet* set = malloc(sizeof(AssertionsSet));
    xmlNode* child = circuitNode->children;
    int i;
    int* tmpTruth = NULL;
    int tmpDepth, tmpValveNo;
    xmlNode** tpNodes = NULL;
    NodeIdMap* nodeMap = createNodeIdMap();
    LinkedListSortingNode* thisNode;
    LinkedListSortingNode* llNode;
    set->nTp = 0;
    
    while(child) {
        if(strEqual(child->name, NODE_NAME_TP)) {
            tpNodes = realloc(tpNodes, (set->nTp + 1) * sizeof(xmlNode*));
            tpNodes[set->nTp] = child;
            set->nTp++;
            if(!nodeHasElementChildren(child)) {
                addInputNodeToIdMap(nodeMap, child);
            }
        }
        if(xmlHasProp(child, ATTR_NAME_ID)) {
            addNodeToIdMap(nodeMap, child);
        }
        child = child->next;
    }
    
    LinkedListSortingNode* ll = NULL;
    for(i = 0; i < set->nTp; i++) {
        tmpTruth = parseNode(tpNodes[i],nodeMap,&tmpDepth,&tmpValveNo);
        assert(tmpTruth != NULL);
        
        llNode = malloc(sizeof(LinkedListSortingNode));
        llNode->next = NULL;
        llNode->prev = NULL;
        llNode->tp = createTestPointFromXMLNode(tpNodes[i], tmpTruth, tmpValveNo);
        llNode->depth = tmpDepth;
        
        if(ll == NULL) {
            ll = llNode;
        } else {
            thisNode = ll;
            while(thisNode != NULL) {
                if(thisNode->depth >= llNode->depth) {
                    if(thisNode->prev != NULL) {
                        thisNode->prev->next = llNode;
                    }
                    llNode->prev = thisNode->prev;
                    llNode->next = thisNode;
                    thisNode->prev = llNode;
                    if(thisNode == ll) {
                        ll = llNode;
                    }
                    break;
                } else if(thisNode->next == NULL) {
                    llNode->prev = thisNode;
                    thisNode->next = llNode;
                    break;
                }
                thisNode = thisNode->next;
            }
            assert(thisNode != NULL);
        }
    }
    
    thisNode = ll;
    set->tps = malloc(set->nTp * sizeof(TestPoint*));
    set->nInputs = nodeMap->nInputs;
    for(i = 0; i < set->nTp; i++) {
        assert(thisNode != NULL);
        set->tps[i] = thisNode->tp;
        if(thisNode->next != NULL) {
            thisNode = thisNode->next;
            free(thisNode->prev);
        } else {
            free(thisNode);
        }
    }
    
    freeNodeIdMap(nodeMap);
    free(tpNodes);
    return set;
}

void freeAssertionSet(AssertionsSet* set) {
    int i;
    if(set != NULL) {
        for(i = 0; i < set->nTp; i++) {
            freeTestPoint(set->tps[i]);
        }
        free(set);
    }
}

int findTableRowForInputs(AssertionsSet* set, int* samples) {
    int i, j, nCombs = 1 << set->nInputs;
    for(j = 0; j < nCombs; j++) {
        for(i = 0; i < set->nInputs; i++) {
            if(set->tps[i]->truth[j] != samples[i]) {
                i = -1;
                break;
            }
        }
        if(i >= 0) {
            return j;
        }
    }
    return -1;
}

void checkTruthTable(AssertionsSet* set, int* samples, int* dest, int* n) {
    int i;
    int row = findTableRowForInputs(set, samples);
    assert(row >= 0);
    *n = 0;
    for(i = set->nInputs; i < set->nTp; i++) {
        if(set->tps[i]->truth[row] != samples[i]) {
            dest[*n] = i;
            (*n)++;
        }
    }
}

void printTPs(AssertionsSet* set) {
    int i, nColumns, nRows, maxCellStringLen;
    char** columns;
    char*** rows;
    assert(set != NULL);
    
    maxCellStringLen = 16;
    nColumns = 5;
    assert((columns = malloc(sizeof(char*) * nColumns)) != NULL);
    columns[0] = TP_TABLE_HEADER_TP;
    columns[1] = TP_TABLE_HEADER_IS_INPUT;
    columns[2] = TP_TABLE_HEADER_VALVE_NO;
    columns[3] = TP_TABLE_HEADER_MIN_V;
    columns[4] = TP_TABLE_HEADER_MAX_V;
    nRows = set->nTp;
    assert((rows = malloc(sizeof(char**) * nRows)) != NULL);
    for(i = 0; i < nRows; i++) {
        rows[i] = malloc(sizeof(char*) * nColumns);
        rows[i][0] = set->tps[i]->tpName;
        rows[i][1] = i < set->nInputs ? YES_STR : NO_STR;
        assert((rows[i][2] = malloc(sizeof(char) * maxCellStringLen)) != NULL);
        if(set->tps[i]->valveNo >= 0) {
            snprintf(rows[i][2], maxCellStringLen, "%d", set->tps[i]->valveNo);
        } else {
            assert(strlen("-")+1 <= maxCellStringLen);
            strcpy(rows[i][2], "-");
        }
        assert((rows[i][3] = malloc(sizeof(char) * maxCellStringLen)) != NULL);
        snprintf(rows[i][3], maxCellStringLen, "%f", set->tps[i]->min);
        assert((rows[i][4] = malloc(sizeof(char) * maxCellStringLen)) != NULL);
        snprintf(rows[i][4], maxCellStringLen, "%f", set->tps[i]->max);
    }
    printTable(stdout, TP_TABLE_TITLE, columns, nColumns, rows, nRows);
    for(i = 0; i < nRows; i++) {
        free(rows[i][2]);
        free(rows[i][3]);
        free(rows[i][4]);
        free(rows[i]);
    }
    free(rows);
    free(columns);
}

void printTruthTable(AssertionsSet* set) {
    int i, j, nColumns, nRows, maxCellStringLen;
    char** columns;
    char*** rows;
    assert(set != NULL);
    
    maxCellStringLen = 8;
    nColumns = set->nTp;
    assert((columns = malloc(sizeof(char*) * nColumns)) != NULL);
    for(i = 0; i < nColumns; i++) {
        columns[i] = set->tps[i]->tpName;
    }
    nRows = 1 << set->nInputs;
    assert((rows = malloc(sizeof(char**) * nRows)) != NULL);
    for(i = 0; i < nRows; i++) {
        rows[i] = malloc(sizeof(char*) * nColumns);
        for(j = 0; j < nColumns; j++) {
            rows[i][j] = malloc(sizeof(char) * maxCellStringLen);
            snprintf(rows[i][j], maxCellStringLen, "%d", set->tps[j]->truth[i]);
        }
    }
    printTable(stdout, TRUTH_TABLE_TITLE, columns, nColumns, rows, nRows);
    for(i = 0; i < nRows; i++) {
        for(j = 0; j < nColumns; j++) {
            free(rows[i][j]);
        }
        free(rows[i]);
    }
    free(rows);
    free(columns);
}