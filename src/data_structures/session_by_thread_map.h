#ifndef SESSION_BY_THREAD_H
#define SESSION_BY_THREAD_H

#include "../static.h"
#include <openssl/ssl.h>

// INIT???

int put(int session_id);
int get(int session_id);

#endif // SESSION_BY_THREAD_H
