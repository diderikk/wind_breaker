#ifndef SESSION_BY_THREAD_H
#define SESSION_BY_THREAD_H

void destroy_session_by_thread_map();

int put_session_id_by_thread(int session_id);
int get_session_id_by_thread();

#endif // SESSION_BY_THREAD_H
