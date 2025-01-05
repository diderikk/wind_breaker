#ifndef SESSION_H
#define SESSION_H

struct session {
  int id;
  int related_fd;
  unsigned long thread_id;
};

int init_session_cache(int _max_size);
void add_thread_to_session(int related_fd);
void remove_thread_from_session();
void add_to_session_sync(int related_fd);
struct session *get_session_for_thread();
void del_from_session_sync(int related_fd);

#endif // SESSION_H
