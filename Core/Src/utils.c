#include "utils.h"
#include <string.h>
#include <stdio.h>

// Trích xuất giá trị chuỗi từ JSON đơn giản
void get_json_string_value(const char *json, const char *key, char *dest, int max_len) {
    dest[0] = '\0';
    char search_key[64];
    sprintf(search_key, "\"%s\"", key);
    char *ptr = strstr(json, search_key);
    if (ptr) {
        char *colon = strchr(ptr + strlen(search_key), ':');
        if (colon) {
            char *start = strchr(colon, '"');
            if (start) {
                start++;
                char *end = strchr(start, '"');
                if (end) {
                    int len = end - start;
                    if (len >= max_len) len = max_len - 1;
                    strncpy(dest, start, len);
                    dest[len] = '\0';
                }
            }
        }
    }
}

// Trích xuất giá trị số nguyên từ JSON đơn giản
int get_json_int_value(const char *json, const char *key, int default_val) {
    char search_key[64];
    sprintf(search_key, "\"%s\"", key);
    char *ptr = strstr(json, search_key);
    if (ptr) {
        char *colon = strchr(ptr + strlen(search_key), ':');
        if (colon) {
            int val;
            if (sscanf(colon + 1, "%d", &val) == 1) {
                return val;
            }
        }
    }
    return default_val;
}
// Trích xuất giá trị (số hoặc chuỗi) từ JSON dưới dạng chuỗi
void get_json_val_as_string(const char *json, const char *key, char *dest, int max_len) {
    dest[0] = '\0';
    char search_key[64];
    sprintf(search_key, "\"%s\"", key);
    char *ptr = strstr(json, search_key);
    if (ptr) {
        char *colon = strchr(ptr + strlen(search_key), ':');
        if (colon) {
            // Bỏ qua khoảng trắng
            char *start = colon + 1;
            while (*start == ' ' || *start == '\t') start++;
            
            if (*start == '"') {
                // Định dạng chuỗi có nháy kép: "key": "value"
                start++;
                char *end = strchr(start, '"');
                if (end) {
                    int len = end - start;
                    if (len >= max_len) len = max_len - 1;
                    strncpy(dest, start, len);
                    dest[len] = '\0';
                }
            } else {
                // Định dạng số không nháy kép: "key": 12345
                char *end = start;
                while (*end && *end != ',' && *end != '}' && *end != '\r' && *end != '\n' && *end != ' ' && *end != '\t') {
                    end++;
                }
                int len = end - start;
                if (len >= max_len) len = max_len - 1;
                strncpy(dest, start, len);
                dest[len] = '\0';
            }
        }
    }
}

// Kiểm tra chuỗi có hoàn toàn là số nguyên (có thể có dấu âm ở đầu) hay không
bool is_string_numeric(const char *str) {
    if (*str == '\0') return false;
    if (*str == '-') str++;
    if (*str == '\0') return false;
    while (*str) {
        if (*str < '0' || *str > '9') return false;
        str++;
    }
    return true;
}
