#ifndef EPOLL_SHIM_CTX_H_
#define EPOLL_SHIM_CTX_H_

#include <sys/tree.h>

#include <stdatomic.h>

#include <signal.h>
#include <unistd.h>

#include "epollfd_ctx.h"
#include "eventfd_ctx.h"
#include "signalfd_ctx.h"
#include "timerfd_ctx.h"

struct file_description_vtable;
typedef struct {
	atomic_int refcount;
	pthread_mutex_t mutex;
	int flags; /* Only for O_NONBLOCK right now. */
	union {
		EpollFDCtx epollfd;
		EventFDCtx eventfd;
		TimerFDCtx timerfd;
		SignalFDCtx signalfd;
	} ctx;
	struct file_description_vtable const *vtable;
} FileDescription;

errno_t file_description_unref(FileDescription **desc);

typedef errno_t (*fd_context_read_fun)(FileDescription *node, int kq, /**/
    void *buf, size_t nbytes, size_t *bytes_transferred);
typedef errno_t (*fd_context_write_fun)(FileDescription *node, int kq, /**/
    void const *buf, size_t nbytes, size_t *bytes_transferred);
typedef errno_t (*fd_context_close_fun)(FileDescription *node);
typedef void (*fd_context_poll_fun)(FileDescription *node, int kq, /**/
    uint32_t *revents);
typedef void (*fd_context_realtime_change_fun)(FileDescription *node, int kq);

struct file_description_vtable {
	fd_context_read_fun read_fun;
	fd_context_write_fun write_fun;
	fd_context_close_fun close_fun;
	fd_context_poll_fun poll_fun;
	fd_context_realtime_change_fun realtime_change_fun;
};

errno_t fd_context_default_read(FileDescription *node, int kq, /**/
    void *buf, size_t nbytes, size_t *bytes_transferred);
errno_t fd_context_default_write(FileDescription *node, int kq, /**/
    void const *buf, size_t nbytes, size_t *bytes_transferred);
PollableNode fd_as_pollable_node(int *fd);

/**/

struct fd_context_map_node_;
typedef struct fd_context_map_node_ FDContextMapNode;

struct fd_context_map_node_ {
	RB_ENTRY(fd_context_map_node_) entry;
	int fd;
	FileDescription *desc;
};

errno_t fd_context_map_node_destroy(FDContextMapNode **node);

/**/

typedef RB_HEAD(fd_context_map_, fd_context_map_node_) FDContextMap;

typedef struct {
	FDContextMap fd_context_map;
	pthread_mutex_t mutex;

	/* members for realtime timer change detection */
	uint64_t nr_fds_for_realtime_step_detector;
	uint64_t realtime_step_detector_generation;
} EpollShimCtx;

extern EpollShimCtx epoll_shim_ctx;

errno_t epoll_shim_ctx_create_node(EpollShimCtx *epoll_shim_ctx, int flags,
    FDContextMapNode **node);
void epoll_shim_ctx_realize_node(EpollShimCtx *epoll_shim_ctx,
    FDContextMapNode *node);

FileDescription *epoll_shim_ctx_find_node(EpollShimCtx *epoll_shim_ctx, int fd);

void epoll_shim_ctx_remove_node_explicit(EpollShimCtx *epoll_shim_ctx,
    FDContextMapNode *node);
void
epoll_shim_ctx_update_realtime_change_monitoring(EpollShimCtx *epoll_shim_ctx,
    int change);

/**/

int epoll_shim_close(int fd);
ssize_t epoll_shim_read(int fd, void *buf, size_t nbytes);
ssize_t epoll_shim_write(int fd, void const *buf, size_t nbytes);

int epoll_shim_poll(struct pollfd *, nfds_t, int);
int epoll_shim_ppoll(struct pollfd *, nfds_t, struct timespec const *,
    sigset_t const *);

int epoll_shim_fcntl(int fd, int cmd, ...);

#endif
