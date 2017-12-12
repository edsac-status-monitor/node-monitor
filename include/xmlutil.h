#ifndef XMLUTIL_H
#define XMLUTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
    
int strEqual(const xmlChar* str1, const char* str2);
int nodePropScanf(xmlNode* node, const char* propName, const char* format, ...);
int nodePropAsInteger(xmlNode* node, const char* propName);
float nodePropAsFloat(xmlNode* node, const char* propName);
int nodePropEqual(xmlNode* node, const char* propName, const char* str2);
int nodeHasElementChildren(xmlNode* node);

#ifdef __cplusplus
}
#endif

#endif /* XMLUTIL_H */

