#ifndef INC_UTILS_H_
#define INC_UTILS_H_

#include <stdint.h>
#include <stdbool.h>
void get_json_string_value(const char *json, const char *key, char *dest, int max_len);
int get_json_int_value(const char *json, const char *key, int default_val);
void get_json_val_as_string(const char *json, const char *key, char *dest, int max_len);
bool is_string_numeric(const char *str);

#endif /* INC_UTILS_H_ */
