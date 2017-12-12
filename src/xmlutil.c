#include <stdarg.h>
#include <libxml/tree.h>
#include <libxml/xmlstring.h>

int strEqual(const xmlChar* str1, const char* str2) {
    return xmlStrEqual(str1, (const xmlChar*) str2);
}

int nodePropScanf(xmlNode* node, const char* propName, const char* format, ...) {
    int i;
    va_list valist;
    xmlChar* propVal;
    
    va_start(valist, format);
    propVal = xmlGetProp(node, propName);
    i = vsscanf(propVal, format, valist);
    xmlFree(propVal);
    va_end(valist);
    return i;
}

int nodePropAsInteger(xmlNode* node, const char* propName) {
    xmlChar* propVal = xmlGetProp(node, propName);
    int intVal = atoi(propVal);
    xmlFree(propVal);
    return intVal;
}

float nodePropAsFloat(xmlNode* node, const char* propName) {
    xmlChar* propVal = xmlGetProp(node, propName);
    float floatVal = atof(propVal);
    xmlFree(propVal);
    return floatVal;
}

int nodePropEqual(xmlNode* node, const char* propName, const char* str2) {
    xmlChar* propVal = xmlGetProp(node, propName);
    int match = strEqual(propVal, str2);
    xmlFree(propVal);
    return match;
}

int nodeHasElementChildren(xmlNode* node) {
    xmlNode* child = node->children;
    while(child) {
        if(child->type == XML_ELEMENT_NODE) {
            return 1;
        }
        child = child->next;
    }
    return 0;
}