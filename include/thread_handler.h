#ifndef THREAD_HANDLER_H
#define THREAD_HANDLER_H

#define WORKER_BUFFER_SIZE 2048

void set_doc_root(const char *root);
void connection_handler(void *arg);

#endif
