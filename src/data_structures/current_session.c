#include "current_session.h"

static __thread int thread_session_id = -1;

void put_current_session(int session_id) { thread_session_id = session_id; }
int get_current_session() { return thread_session_id; }
void clear_current_session() { thread_session_id = -1; }
