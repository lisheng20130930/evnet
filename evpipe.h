#ifndef EVPIPE_H
#define EVPIPE_H

void evpipe_dummy(struct aeEventLoop *eventLoop, fd_t fd, void *clientdata, int mask);
int ev_pipe(int fd[2]);

int pipe_write(int fd, void *buffer, unsigned int size);
int pipe_read(int fd, void *buffer, unsigned int size);

#endif