#pragma once
#include <stddef.h>
#include <stdint.h>
static const size_t WEB_PRINT_LIMIT = 8192;
// nullptr means the upload reservation succeeded; otherwise a user-readable reason.
const char *bridge_web_begin();
bool bridge_web_append(uint8_t byte);
bool bridge_web_commit(size_t *bytes, size_t *removed);
void bridge_web_abort_upload();
