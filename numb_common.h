#ifndef __NUMB_COMMON_H
#define __NUMB_COMMON_H

#include <linux/cdev.h>
#include <linux/wait.h>

struct numb_pipe {
	struct cdev cdev;
	void *buf;
	size_t len;
	size_t head, tail;
	struct mutex lock;
	wait_queue_head_t readq;
    wait_queue_head_t writeq;
};

#define NUMBPIPE_IOC_MAGIC  'N'
#define NUMBPIPE_SET_BLOCKING _IO(NUMBPIPE_IOC_MAGIC, 1)
#define NUMBPIPE_UNSET_BLOCKING _IO(NUMBPIPE_IOC_MAGIC, 2)

static inline bool numb_readbuf_empty(struct numb_pipe *p)
{
	return p->head == p->tail;
}

static inline bool numb_writebuf_empty(struct numb_pipe *p)
{
	return !(p->head < p->tail + p->len);
}

#endif