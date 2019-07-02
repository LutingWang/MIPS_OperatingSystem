#include "lib.h"
#include <mmu.h>
#include <env.h>
#define debug 0

static int pipeclose(struct Fd*);
static int piperead(struct Fd *fd, void *buf, u_int n, u_int offset);
static int pipestat(struct Fd*, struct Stat*);
static int pipewrite(struct Fd *fd, const void *buf, u_int n, u_int offset);

struct Dev devpipe = {
        .dev_id=        'p',
        .dev_name=      "pipe",
        .dev_read=      piperead,
        .dev_write=     pipewrite,
        .dev_close=     pipeclose,
        .dev_stat=      pipestat,
};

#define BY2PIPE 32              // small to provoke races

struct Pipe {
        u_int p_rpos;           // read position
        u_int p_wpos;           // write position
        char p_buf[BY2PIPE];    // data buffer
};

int pipe(int pfd[2]) {
        int r, va;
        struct Fd *fd0, *fd1;

        // allocate the file descriptor table entries
        if ((r = fd_alloc(&fd0)) < 0 || (r = syscall_mem_alloc(0, (u_int)fd0, PTE_V|PTE_R|PTE_LIBRARY)) < 0)
                goto err;

        if ((r = fd_alloc(&fd1)) < 0 || (r = syscall_mem_alloc(0, (u_int)fd1, PTE_V|PTE_R|PTE_LIBRARY)) < 0)
                goto err1;

        // allocate the pipe structure as first data page in both
        va = fd2data(fd0);
        if ((r = syscall_mem_alloc(0, va, PTE_V|PTE_R|PTE_LIBRARY)) < 0)
                goto err2;
        if ((r = syscall_mem_map(0, va, 0, fd2data(fd1), PTE_V|PTE_R|PTE_LIBRARY)) < 0)
                goto err3;

        // set up fd structures
        fd0->fd_dev_id = devpipe.dev_id;
        fd0->fd_omode = O_RDONLY;

        fd1->fd_dev_id = devpipe.dev_id;
        fd1->fd_omode = O_WRONLY;

        writef("[%08x] pipecreate \n", env->env_id, (* vpt)[VPN(va)]);

        pfd[0] = fd2num(fd0);
        pfd[1] = fd2num(fd1);
        return 0;

err3:   syscall_mem_unmap(0, va);
err2:   syscall_mem_unmap(0, (u_int)fd1);
err1:   syscall_mem_unmap(0, (u_int)fd0);
err:    return r;
}

// Check pageref(fd) and pageref(p),
// returning 1 if they're the same, 0 otherwise.
//
// The logic here is that pageref(p) is the total
// number of readers *and* writers, whereas pageref(fd)
// is the number of file descriptors like fd (readers if fd is
// a reader, writers if fd is a writer).
//
// If the number of file descriptors like fd is equal
// to the total number of readers and writers, then
// everybody left is what fd is.  So the other end of
// the pipe is closed.
static int _pipeisclosed(struct Fd *fd, struct Pipe *p) {
        int pfd, pfp, runs;

        do {
                runs = env->env_runs;
                pfd = pageref(fd);
                pfp = pageref(p);
        } while (runs != env->env_runs);

#ifdef DEBUG
        writef("_pipeisclosed@pipe.c: pfd is %x, pfp is %x\n", pfd, pfp);
        writef("_pipeisclosed@pipe.c: pipe is ");
        if (pfd != pfp) writef("not ");
        writef("closed\n");
#endif
        if (pfd == pfp) {
                return 1;
        } else {
                return 0;
        }
        // user_panic("_pipeisclosed not implemented");
}

int pipeisclosed(int fdnum) {
        struct Fd *fd;
        struct Pipe *p;
        int r;

        if ((r = fd_lookup(fdnum, &fd)) < 0)
                return r;
        p = (struct Pipe*)fd2data(fd);
        return _pipeisclosed(fd, p);
}

// See the lab text for a description of
// what piperead needs to do.  Write a loop that
// transfers one byte at a time.  If you decide you need
// to yield (because the pipe is empty), only yield if
// you have not yet copied any bytes.  (If you have copied
// some bytes, return what you have instead of yielding.)
// If the pipe is empty and closed and you didn't copy any data out, return 0.
// Use _pipeisclosed to check whether the pipe is closed.
static int piperead(struct Fd *fd, void *vbuf, u_int n, u_int offset) {
#ifdef DEBUG
        writef("piperead@pipe.c called with (struct Fd *fd: %x, const void *vbuf: %x, u_int n: %x, u_int offset: %x)\n", fd, vbuf, n, offset);
#endif
        int i = 0;
        struct Pipe *p = (struct Pipe *) fd2data(fd);
        char *rbuf = (char *) vbuf;

        while (p->p_rpos >= p->p_wpos) {
#ifdef DEBUG
                writef("piperead@pipe.c: nothing to read\n");
#endif
                if (_pipeisclosed(fd, p)) {
                        return 0;
                } else {
                        syscall_yield();
                }
        }
        while (i++ < n && p->p_rpos < p->p_wpos) {
                *(rbuf++) = p->p_buf[p->p_rpos++ % BY2PIPE];
#ifdef DEBUG
                writef("piperead@pipe.c: read %x %c\n", *(rbuf-1), *(rbuf-1));
#endif
        }
        *rbuf = 0;
#ifdef DEBUG
        writef("piperead@pipe.c: read %x of %x bytes\n", i, n);
#endif
        return i;
        // user_panic("piperead not implemented");
}

// See the lab text for a description of what
// pipewrite needs to do.  Write a loop that transfers one byte
// at a time.  Unlike in read, it is not okay to write only some
// of the data.  If the pipe fills and you've only copied some of
// the data, wait for the pipe to empty and then keep copying.
// If the pipe is full and closed, return 0.
// Use _pipeisclosed to check whether the pipe is closed.
static int pipewrite(struct Fd *fd, const void *vbuf, u_int n, u_int offset) {
#ifdef DEBUG
        writef("pipewrite@pipe.c called with (struct Fd *fd: %x, const void *vbuf: %x(%s), u_int n: %x, u_int offset: %x)\n", fd, vbuf, (char *) vbuf, n, offset);
#endif
        int i = 0;
        struct Pipe *p = (struct Pipe *) fd2data(fd);
        char *wbuf = (char *) vbuf;

        while (i++ < n) {
                while (p->p_wpos - p->p_rpos >= BY2PIPE) {
                        if (_pipeisclosed(fd, p)) {
#ifdef DEBUG
                                writef("pipewrite@pipe.c: pipe is closed, returning 0\n");
#endif
                                return 0;
                        } else {
                                syscall_yield();
                        }
                }
#ifdef DEBUG
                writef("pipewrite@pipe.c: wrote %x %c\n", *wbuf, *wbuf);
#endif
                p->p_buf[p->p_wpos++ % BY2PIPE] = *(wbuf++);
        }
        i--;
#ifdef DEBUG
        writef("pipewrite@pipe.c: wrote %x of %x bytes\n", i, n);
#endif
        return i;
        // user_panic("pipewrite not implemented");
}

static int pipestat(struct Fd *fd, struct Stat *stat) {
        // struct Pipe *p;
        user_panic("pipestat@pipe.c did not implement\n");
        return 0;
}

static int pipeclose(struct Fd *fd) {
        syscall_mem_unmap(0, (u_int) fd);
        syscall_mem_unmap(0, fd2data(fd));
        return 0;
}

