#include "response.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void send_json_response(struct mg_connection *c, int status,
                        const char *json_data) {
  mg_http_reply(c, status,
                "Content-Type: application/json\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Access-Control-Allow-Headers: Content-Type, Authorization\r\n",
                "%s", json_data);
}

void send_success_response(struct mg_connection *c, const char *message,
                           const char *data) {
  char response[1024];
  if (data) {
    snprintf(response, sizeof(response),
             "{"
             "\"success\": true,"
             "\"message\": \"%s\","
             "\"data\": %s"
             "}",
             message, data);
  } else {
    snprintf(response, sizeof(response),
             "{"
             "\"success\": true,"
             "\"message\": \"%s\""
             "}",
             message);
  }
  send_json_response(c, 200, response);
}

void send_error_response(struct mg_connection *c, int status, const char *error,
                         const char *message) {
  char response[512];
  snprintf(response, sizeof(response),
           "{"
           "\"success\": false,"
           "\"error\": \"%s\","
           "\"message\": \"%s\""
           "}",
           error, message);
  send_json_response(c, status, response);
}

void send_data_response(struct mg_connection *c, const char *key,
                        const char *value) {
  char response[512];
  snprintf(response, sizeof(response),
           "{"
           "\"success\": true,"
           "\"%s\": \"%s\""
           "}",
           key, value);
  send_json_response(c, 200, response);
}

char *build_json_object(const char *key_value_pairs[], int count) {
  char *json = malloc(2048);
  strcpy(json, "{");

  for (int i = 0; i < count; i += 2) {
    if (i > 0)
      strcat(json, ",");
    strcat(json, "\"");
    strcat(json, key_value_pairs[i]);
    strcat(json, "\":\"");
    strcat(json, key_value_pairs[i + 1]);
    strcat(json, "\"");
  }

  strcat(json, "}");
  return json;
}

char *build_json_array(const char *items[], int count) {
  char *json = malloc(1024);
  strcpy(json, "[");

  for (int i = 0; i < count; i++) {
    if (i > 0)
      strcat(json, ",");
    strcat(json, "\"");
    strcat(json, items[i]);
    strcat(json, "\"");
  }

  strcat(json, "]");
  return json;
}

void free_json_string(char *json_str) {
  if (json_str)
    free(json_str);
}
