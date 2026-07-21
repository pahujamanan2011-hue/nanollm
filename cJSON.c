/* cJSON.c - Minimal JSON parser/builder implementation */
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

static cJSON* cJSON_New_Item(void) {
    return (cJSON*)calloc(1, sizeof(cJSON));
}

void cJSON_Delete(cJSON *c) {
    cJSON *next;
    while (c) {
        next = c->next;
        if (c->child) cJSON_Delete(c->child);
        if (c->valuestring) free(c->valuestring);
        if (c->string) free(c->string);
        free(c);
        c = next;
    }
}

static const char* skip(const char *in) {
    while (in && *in && (unsigned char)*in <= 32) in++;
    return in;
}

static const char* parse_string(cJSON *item, const char *str) {
    const char *ptr = str + 1;
    char *ptr2;
    char *out;
    int len = 0;
    if (*str != '\"') return NULL;
    while (*ptr != '\"' && *ptr) {
        len++;
        if (*ptr++ == '\\') ptr++;
    }
    out = (char*)malloc(len + 1);
    if (!out) return NULL;
    ptr = str + 1;
    ptr2 = out;
    while (*ptr != '\"' && *ptr) {
        if (*ptr != '\\') {
            *ptr2++ = *ptr++;
        } else {
            ptr++;
            switch (*ptr) {
                case 'b': *ptr2++ = '\b'; break;
                case 'f': *ptr2++ = '\f'; break;
                case 'n': *ptr2++ = '\n'; break;
                case 'r': *ptr2++ = '\r'; break;
                case 't': *ptr2++ = '\t'; break;
                case 'u': ptr += 4; *ptr2++ = '?'; break;
                default: *ptr2++ = *ptr; break;
            }
            ptr++;
        }
    }
    *ptr2 = '\0';
    if (*ptr == '\"') ptr++;
    item->valuestring = out;
    item->type = cJSON_String;
    return ptr;
}

static const char* parse_number(cJSON *item, const char *num) {
    double n = 0, sign = 1, scale = 0;
    int subscale = 0, signsubscale = 1;
    if (*num == '-') sign = -1, num++;
    if (*num == '0') num++;
    if (*num >= '1' && *num <= '9') {
        do { n = (n * 10.0) + (*num++ - '0'); } while (*num >= '0' && *num <= '9');
    }
    if (*num == '.') {
        num++;
        do { n = (n * 10.0) + (*num++ - '0'); scale--; } while (*num >= '0' && *num <= '9');
    }
    if (*num == 'e' || *num == 'E') {
        num++;
        if (*num == '+') num++;
        else if (*num == '-') signsubscale = -1, num++;
        while (*num >= '0' && *num <= '9') subscale = (subscale * 10) + (*num++ - '0');
    }
    n = sign * n * pow(10.0, (scale + subscale * signsubscale));
    item->valuedouble = n;
    item->valueint = (int)n;
    item->type = cJSON_Number;
    return num;
}

static const char* parse_value(cJSON *item, const char *value);

static const char* parse_array(cJSON *item, const char *value) {
    cJSON *child;
    if (*value != '[') return NULL;
    item->type = cJSON_Array;
    value = skip(value + 1);
    if (*value == ']') return value + 1;
    item->child = child = cJSON_New_Item();
    if (!item->child) return NULL;
    value = skip(parse_value(child, skip(value)));
    if (!value) return NULL;
    while (*value == ',') {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) return NULL;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip(parse_value(child, skip(value + 1)));
        if (!value) return NULL;
    }
    if (*value == ']') return value + 1;
    return NULL;
}

static const char* parse_object(cJSON *item, const char *value) {
    cJSON *child;
    if (*value != '{') return NULL;
    item->type = cJSON_Object;
    value = skip(value + 1);
    if (*value == '}') return value + 1;
    item->child = child = cJSON_New_Item();
    if (!item->child) return NULL;
    value = skip(parse_string(child, skip(value)));
    if (!value) return NULL;
    child->string = child->valuestring;
    child->valuestring = NULL;
    value = skip(value);
    if (*value != ':') return NULL;
    value = skip(parse_value(child, skip(value + 1)));
    if (!value) return NULL;
    while (*value == ',') {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) return NULL;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip(parse_string(child, skip(value + 1)));
        if (!value) return NULL;
        child->string = child->valuestring;
        child->valuestring = NULL;
        value = skip(value);
        if (*value != ':') return NULL;
        value = skip(parse_value(child, skip(value + 1)));
        if (!value) return NULL;
    }
    if (*value == '}') return value + 1;
    return NULL;
}

static const char* parse_value(cJSON *item, const char *value) {
    if (!value) return NULL;
    if (!strncmp(value, "null", 4)) { item->type = cJSON_NULL; return value + 4; }
    if (!strncmp(value, "false", 5)) { item->type = cJSON_False; return value + 5; }
    if (!strncmp(value, "true", 4)) { item->type = cJSON_True; return value + 4; }
    if (*value == '\"') return parse_string(item, value);
    if (*value == '-' || (*value >= '0' && *value <= '9')) return parse_number(item, value);
    if (*value == '[') return parse_array(item, value);
    if (*value == '{') return parse_object(item, value);
    return NULL;
}

cJSON* cJSON_Parse(const char *value) {
    cJSON *c = cJSON_New_Item();
    if (!c) return NULL;
    if (!parse_value(c, skip(value))) {
        cJSON_Delete(c);
        return NULL;
    }
    return c;
}

static char* print_number(cJSON *item) {
    char *str = (char*)malloc(64);
    if (!str) return NULL;
    if (fabs(((double)item->valueint) - item->valuedouble) <= 0.000001) {
        sprintf(str, "%d", item->valueint);
    } else {
        sprintf(str, "%.15g", item->valuedouble);
    }
    return str;
}

static char* print_string(const char *str) {
    const char *ptr;
    char *ptr2, *out;
    int len = 0;
    unsigned char token;
    if (!str) {
        out = (char*)malloc(3);
        strcpy(out, "\"\"");
        return out;
    }
    ptr = str;
    while (*ptr) {
        token = (unsigned char)*ptr++;
        if (token == '\"' || token == '\\' || token == '\b' || token == '\f' ||
            token == '\n' || token == '\r' || token == '\t') {
            len += 2;
        } else if (token < 32) {
            len += 6;
        } else {
            len++;
        }
    }
    out = (char*)malloc(len + 3);
    if (!out) return NULL;
    ptr2 = out;
    ptr = str;
    *ptr2++ = '\"';
    while (*ptr) {
        token = (unsigned char)*ptr++;
        switch (token) {
            case '\"': *ptr2++ = '\\'; *ptr2++ = '\"'; break;
            case '\\': *ptr2++ = '\\'; *ptr2++ = '\\'; break;
            case '\b': *ptr2++ = '\\'; *ptr2++ = 'b'; break;
            case '\f': *ptr2++ = '\\'; *ptr2++ = 'f'; break;
            case '\n': *ptr2++ = '\\'; *ptr2++ = 'n'; break;
            case '\r': *ptr2++ = '\\'; *ptr2++ = 'r'; break;
            case '\t': *ptr2++ = '\\'; *ptr2++ = 't'; break;
            default:
                if (token < 32) {
                    sprintf(ptr2, "\\u%04x", token);
                    ptr2 += 6;
                } else {
                    *ptr2++ = token;
                }
        }
    }
    *ptr2++ = '\"';
    *ptr2 = '\0';
    return out;
}

static char* print_value(cJSON *item, int depth, int fmt);

static char* print_array(cJSON *item, int depth, int fmt) {
    char **entries;
    char *out = NULL, *ptr, *ret;
    int len = 5;
    cJSON *child = item->child;
    int numentries = 0, i = 0, j;
    int fail = 0;
    while (child) { numentries++; child = child->next; }
    if (!numentries) {
        out = (char*)malloc(3);
        strcpy(out, "[]");
        return out;
    }
    entries = (char**)calloc(numentries, sizeof(char*));
    if (!entries) return NULL;
    child = item->child;
    while (child && !fail) {
        ret = print_value(child, depth + 1, fmt);
        entries[i++] = ret;
        if (ret) len += (int)strlen(ret) + 2 + (fmt ? 1 : 0);
        else fail = 1;
        child = child->next;
    }
    if (!fail) out = (char*)malloc(len);
    if (!out) fail = 1;
    if (!fail) {
        ptr = out;
        *ptr++ = '[';
        if (fmt) { *ptr++ = '\n'; for (j = 0; j < depth + 1; j++) { *ptr++ = '\t'; } }
        for (i = 0; i < numentries; i++) {
            strcpy(ptr, entries[i]);
            ptr += strlen(entries[i]);
            if (i != numentries - 1) {
                *ptr++ = ',';
                if (fmt) { *ptr++ = '\n'; for (j = 0; j < depth + 1; j++) { *ptr++ = '\t'; } }
            }
            free(entries[i]);
        }
        if (fmt) { *ptr++ = '\n'; for (j = 0; j < depth; j++) { *ptr++ = '\t'; } }
        *ptr++ = ']';
        *ptr = '\0';
    } else {
        for (i = 0; i < numentries; i++) free(entries[i]);
    }
    free(entries);
    return out;
}

static char* print_object(cJSON *item, int depth, int fmt) {
    char **entries = NULL, **names = NULL;
    char *out = NULL, *ptr, *ret;
    int len = 7, i = 0, j;
    cJSON *child = item->child;
    int numentries = 0;
    int fail = 0;
    while (child) { numentries++; child = child->next; }
    if (!numentries) {
        out = (char*)malloc(3);
        strcpy(out, "{}");
        return out;
    }
    entries = (char**)calloc(numentries, sizeof(char*));
    names = (char**)calloc(numentries, sizeof(char*));
    if (!entries || !names) { free(entries); free(names); return NULL; }
    child = item->child;
    while (child && !fail) {
        names[i] = print_string(child->string);
        ret = print_value(child, depth + 1, fmt);
        entries[i] = ret;
        if (names[i] && ret) {
            len += (int)strlen(ret) + (int)strlen(names[i]) + 2 + (fmt ? 2 : 0);
        } else {
            fail = 1;
        }
        i++;
        child = child->next;
    }
    if (!fail) out = (char*)malloc(len);
    if (!out) fail = 1;
    if (!fail) {
        ptr = out;
        *ptr++ = '{';
        if (fmt) { *ptr++ = '\n'; for (j = 0; j < depth + 1; j++) { *ptr++ = '\t'; } }
        for (i = 0; i < numentries; i++) {
            strcpy(ptr, names[i]);
            ptr += strlen(names[i]);
            free(names[i]);
            *ptr++ = ':';
            if (fmt) *ptr++ = ' ';
            strcpy(ptr, entries[i]);
            ptr += strlen(entries[i]);
            free(entries[i]);
            if (i != numentries - 1) {
                *ptr++ = ',';
                if (fmt) { *ptr++ = '\n'; for (j = 0; j < depth + 1; j++) { *ptr++ = '\t'; } }
            }
        }
        if (fmt) { *ptr++ = '\n'; for (j = 0; j < depth; j++) { *ptr++ = '\t'; } }
        *ptr++ = '}';
        *ptr = '\0';
    } else {
        for (i = 0; i < numentries; i++) {
            free(names[i]);
            free(entries[i]);
        }
    }
    free(names);
    free(entries);
    return out;
}

static char* print_value(cJSON *item, int depth, int fmt) {
    if (!item) return NULL;
    switch (item->type) {
        case cJSON_NULL: return strdup("null");
        case cJSON_False: return strdup("false");
        case cJSON_True: return strdup("true");
        case cJSON_Number: return print_number(item);
        case cJSON_String: return print_string(item->valuestring);
        case cJSON_Array: return print_array(item, depth, fmt);
        case cJSON_Object: return print_object(item, depth, fmt);
        default: return NULL;
    }
}

char* cJSON_Print(const cJSON *item) {
    return print_value((cJSON*)item, 0, 1);
}

char* cJSON_PrintUnformatted(const cJSON *item) {
    return print_value((cJSON*)item, 0, 0);
}

cJSON* cJSON_CreateNull(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_NULL; return item; }
cJSON* cJSON_CreateTrue(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_True; return item; }
cJSON* cJSON_CreateFalse(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_False; return item; }
cJSON* cJSON_CreateBool(int b) { cJSON *item = cJSON_New_Item(); if (item) item->type = b ? cJSON_True : cJSON_False; return item; }
cJSON* cJSON_CreateNumber(double num) { cJSON *item = cJSON_New_Item(); if (item) { item->type = cJSON_Number; item->valuedouble = num; item->valueint = (int)num; } return item; }
cJSON* cJSON_CreateString(const char *string) { cJSON *item = cJSON_New_Item(); if (item) { item->type = cJSON_String; item->valuestring = strdup(string); } return item; }
cJSON* cJSON_CreateArray(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_Array; return item; }
cJSON* cJSON_CreateObject(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_Object; return item; }

cJSON* cJSON_AddItemToArray(cJSON *array, cJSON *item) {
    cJSON *c;
    if (!array || !item) return NULL;
    c = array->child;
    if (!c) { array->child = item; }
    else {
        while (c && c->next) c = c->next;
        c->next = item;
        item->prev = c;
    }
    return item;
}

cJSON* cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item) {
    if (!object || !string || !item) return NULL;
    if (item->string) free(item->string);
    item->string = strdup(string);
    cJSON_AddItemToArray(object, item);
    return item;
}

cJSON* cJSON_AddNullToObject(cJSON *object, const char *name) { return cJSON_AddItemToObject(object, name, cJSON_CreateNull()); }
cJSON* cJSON_AddTrueToObject(cJSON *object, const char *name) { return cJSON_AddItemToObject(object, name, cJSON_CreateTrue()); }
cJSON* cJSON_AddFalseToObject(cJSON *object, const char *name) { return cJSON_AddItemToObject(object, name, cJSON_CreateFalse()); }
cJSON* cJSON_AddBoolToObject(cJSON *object, const char *name, int b) { return cJSON_AddItemToObject(object, name, cJSON_CreateBool(b)); }
cJSON* cJSON_AddNumberToObject(cJSON *object, const char *name, double n) { return cJSON_AddItemToObject(object, name, cJSON_CreateNumber(n)); }
cJSON* cJSON_AddStringToObject(cJSON *object, const char *name, const char *s) { return cJSON_AddItemToObject(object, name, cJSON_CreateString(s)); }

cJSON* cJSON_GetObjectItem(const cJSON *object, const char *string) {
    cJSON *c;
    if (!object || !string) return NULL;
    c = object->child;
    while (c) {
        if (c->string && !strcmp(c->string, string)) return c;
        c = c->next;
    }
    return NULL;
}

cJSON* cJSON_GetArrayItem(const cJSON *array, int index) {
    cJSON *c;
    if (!array) return NULL;
    c = array->child;
    while (c && index > 0) { c = c->next; index--; }
    return c;
}

int cJSON_GetArraySize(const cJSON *array) {
    cJSON *c;
    int i = 0;
    if (!array) return 0;
    c = array->child;
    while (c) { i++; c = c->next; }
    return i;
}
