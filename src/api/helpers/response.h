#ifndef RESPONSE_H
#define RESPONSE_H

#include "../../mongoose/mongoose.h"

// Response helper functions
void send_json_response(struct mg_connection *c, int status,
                        const char *json_data);
void send_success_response(struct mg_connection *c, const char *message,
                           const char *data);
void send_error_response(struct mg_connection *c, int status, const char *error,
                         const char *message);
void send_data_response(struct mg_connection *c, const char *key,
                        const char *value);

// Response builders
char *build_json_object(const char *key_value_pairs[], int count);
char *build_json_array(const char *items[], int count);

// Memory management
void free_json_string(char *json_str);

#endif // RESPONSE_H
