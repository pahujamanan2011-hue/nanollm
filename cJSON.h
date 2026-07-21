/* cJSON.h - Minimal JSON parser/builder */
#ifndef CJSON_H
#define CJSON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#define cJSON_Invalid 0
#define cJSON_False   1
#define cJSON_True    2
#define cJSON_NULL    3
#define cJSON_Number  4
#define cJSON_String  5
#define cJSON_Array   6
#define cJSON_Object  7

typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    int type;
    char *valuestring;
    int valueint;
    double valuedouble;
    char *string;
} cJSON;

cJSON* cJSON_Parse(const char *value);
char* cJSON_Print(const cJSON *item);
char* cJSON_PrintUnformatted(const cJSON *item);
void cJSON_Delete(cJSON *item);

cJSON* cJSON_CreateNull(void);
cJSON* cJSON_CreateTrue(void);
cJSON* cJSON_CreateFalse(void);
cJSON* cJSON_CreateBool(int b);
cJSON* cJSON_CreateNumber(double num);
cJSON* cJSON_CreateString(const char *string);
cJSON* cJSON_CreateArray(void);
cJSON* cJSON_CreateObject(void);

cJSON* cJSON_AddNullToObject(cJSON *object, const char *name);
cJSON* cJSON_AddTrueToObject(cJSON *object, const char *name);
cJSON* cJSON_AddFalseToObject(cJSON *object, const char *name);
cJSON* cJSON_AddBoolToObject(cJSON *object, const char *name, int b);
cJSON* cJSON_AddNumberToObject(cJSON *object, const char *name, double n);
cJSON* cJSON_AddStringToObject(cJSON *object, const char *name, const char *s);
cJSON* cJSON_AddItemToArray(cJSON *array, cJSON *item);
cJSON* cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);

cJSON* cJSON_GetObjectItem(const cJSON *object, const char *string);
cJSON* cJSON_GetArrayItem(const cJSON *array, int index);
int cJSON_GetArraySize(const cJSON *array);

#define cJSON_IsInvalid(item) ((item) == NULL || (item)->type == cJSON_Invalid)
#define cJSON_IsFalse(item)   ((item) != NULL && (item)->type == cJSON_False)
#define cJSON_IsTrue(item)    ((item) != NULL && (item)->type == cJSON_True)
#define cJSON_IsBool(item)    ((item) != NULL && ((item)->type == cJSON_True || (item)->type == cJSON_False))
#define cJSON_IsNull(item)    ((item) != NULL && (item)->type == cJSON_NULL)
#define cJSON_IsNumber(item)  ((item) != NULL && (item)->type == cJSON_Number)
#define cJSON_IsString(item)  ((item) != NULL && (item)->type == cJSON_String)
#define cJSON_IsArray(item)   ((item) != NULL && (item)->type == cJSON_Array)
#define cJSON_IsObject(item)  ((item) != NULL && (item)->type == cJSON_Object)

#ifdef __cplusplus
}
#endif

#endif
