/*
 * File system server main loop -
 * serves IPC requests from other environments.
 */

#include "fs.h"
#include "fd.h"
#include "lib.h"
#include <mmu.h>

struct Open {
        struct File *o_file;    // mapped descriptor for open file
        u_int o_fileid;                 // file id
        int o_mode;                             // open mode
        struct Filefd *o_ff;    // va of filefd page
};

// Max number of open files in the file system at once
#define MAXOPEN                 1024
#define FILEVA                  0x60000000

// initialize to force into data section
struct Open opentab[MAXOPEN] = { { 0, 0, 1 } };

// Virtual address at which to receive page mappings containing client requests.
#define REQVA   0x0ffff000

static inline void print_struct_Open(int i) {
        struct Open *tmp = opentab + i;
        writef("opentab[%x] = {struct File *o_file=%x, u_int o_fileid=%x, int o_mode=%x, struct Filefd *o_ff=%x}", i, tmp->o_file, tmp->o_fileid, tmp->o_mode, tmp->o_ff);
}

// Overview:
//      Initialize file system server process.
void serve_init(void) {
#ifdef DEBUG
        writef("serve_init@serv.c called\n");
#endif
        int i;
        u_int va;

        // Set virtual address to map.
        va = FILEVA;

        // Initial array opentab.
        for (i = 0; i < MAXOPEN; i++) {
                opentab[i].o_fileid = i;
                opentab[i].o_ff = (struct Filefd *)va;
                va += BY2PG;
        }
#ifdef DEBUG
        writef("serve_init@serv.c: initiated opentab\n");
#endif
}

// Overview:
//      Allocate an open file.
int open_alloc(struct Open **o) {
        int i, r;

        // Find an available open-file table entry
        for (i = 0; i < MAXOPEN; i++) {
#ifdef DEBUG
                writef("open_alloc@serv.c: ");
                print_struct_Open(i);
                writef("\n");
#endif
                switch (pageref(opentab[i].o_ff)) {
                        case 0:
#ifdef DEBUG
                                writef("open_alloc@serv.c: pageref of %x is 0, allocing a new page for it\n", opentab[i].o_ff);
#endif
                                if ((r = syscall_mem_alloc(0, (u_int)opentab[i].o_ff, PTE_V | PTE_R | PTE_LIBRARY)) < 0) {
                                        return r;
                                }
                        case 1:
#ifdef DEBUG
                                writef("open_alloc@serv.c: pageref of %x is 1, returning this struct with file id %x\n", opentab[i].o_ff, opentab[i].o_fileid + MAXOPEN);
#endif
                                opentab[i].o_fileid += MAXOPEN;
                                *o = &opentab[i];
                                user_bzero((void *)opentab[i].o_ff, BY2PG);
                                return (*o)->o_fileid;
                }
        }

        return -E_MAX_OPEN;
}

// Overview:
//      Look up an open file for envid.
int open_lookup(u_int envid, u_int fileid, struct Open **po) {
#ifdef DEBUG
        writef("open_lookup@serv.c called with (u_int envid: %x, u_int fileid: %x, struct Open **po)\n", envid, fileid);
#endif
        struct Open *o;

        o = &opentab[fileid % MAXOPEN];

#ifdef DEBUG
        writef("open_lookup@serv.c: fetched struct Open ");
        print_struct_Open(fileid % MAXOPEN);
        writef(", whose pageref is %x\n", pageref(o->o_ff));
#endif
        if (pageref(o->o_ff) == 1 || o->o_fileid != fileid) {
                return -E_INVAL;
        }

        *po = o;
        return 0;
}

// Serve requests, sending responses back to envid.
// To send a result back, ipc_send(envid, r, 0, 0).
// To include a page, ipc_send(envid, r, srcva, perm).

void serve_open(u_int envid, struct Fsreq_open *rq) {
        writef("serve_open %08x %x 0x%x\n", envid, (int)rq->req_path, rq->req_omode);

        u_char path[MAXPATHLEN];
        struct File *f;
        struct Filefd *ff;
        int fileid;
        int r;
        struct Open *o;

        // Copy in the path, making sure it's null-terminated
        user_bcopy(rq->req_path, path, MAXPATHLEN);
        path[MAXPATHLEN - 1] = 0;

        // Find a file id.
        if ((r = open_alloc(&o)) < 0) {
                user_panic("open_alloc failed: %d, invalid path: %s", r, path);
                ipc_send(envid, r, 0, 0);
        }

        fileid = r;

        // Open the file.
        if ((r = file_open((char *)path, &f)) < 0) {
        //      user_panic("file_open failed: %d, invalid path: %s", r, path);
                ipc_send(envid, r, 0, 0);
                return ;
        }

        // Save the file pointer.
        o->o_file = f;

        // Fill out the Filefd structure
        ff = (struct Filefd *)o->o_ff;
        ff->f_file = *f;
        ff->f_fileid = o->o_fileid;
        o->o_mode = rq->req_omode;
        ff->f_fd.fd_omode = o->o_mode;
        ff->f_fd.fd_dev_id = devfile.dev_id;

#ifdef DEBUG
        writef("serve_open@serv.c: Filefd structure filled out, mapping back to caller\n");
#endif
        ipc_send(envid, 0, (u_int) o->o_ff, PTE_V | PTE_R | PTE_LIBRARY);
}

void serve_map(u_int envid, struct Fsreq_map *rq) {
        struct Open *pOpen;

        u_int filebno;

        void *blk;

        int r;

        if ((r = open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
                ipc_send(envid, r, 0, 0);
                return;
        }

        filebno = rq->req_offset / BY2BLK;

        if ((r = file_get_block(pOpen->o_file, filebno, &blk)) < 0) {
                ipc_send(envid, r, 0, 0);
                return;
        }

        ipc_send(envid, 0, (u_int)blk, PTE_V | PTE_R | PTE_LIBRARY);
}

void serve_set_size(u_int envid, struct Fsreq_set_size *rq) {
        struct Open *pOpen;
        int r;
        if ((r = open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
                ipc_send(envid, r, 0, 0);
                return;
        }

        if ((r = file_set_size(pOpen->o_file, rq->req_size)) < 0) {
                ipc_send(envid, r, 0, 0);
                return;
        }

        ipc_send(envid, 0, 0, 0);
}

void serve_close(u_int envid, struct Fsreq_close *rq) {
#ifdef DEBUG
        writef("serve_close@serv.c called with (u_int envid: %x, struct Fsreq_close *rq: %x)\n", envid, rq);
#endif
        struct Open *pOpen;

        int r;

        if ((r = open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
                ipc_send(envid, r, 0, 0);
                return;
        }
        file_close(pOpen->o_file);
        ipc_send(envid, 0, 0, 0);
}

// Overview:
//      fs service used to delete a file according path in `rq`.
void serve_remove(u_int envid, struct Fsreq_remove *rq) {
#ifdef DEBUG
        writef("serve_remove@serv.c called with (u_int envid: %x, struct Fsreq_remove *rq: %x)\n", envid, rq);
#endif
        int r;
        u_char path[MAXPATHLEN];

        user_bcopy(rq->req_path, path, MAXPATHLEN);
        path[MAXPATHLEN - 1] = 0;
        if ((r = file_remove((char *) path))) {
                ipc_send(envid, r, 0, 0);
                return;
        }
        ipc_send(envid, 0, 0, 0);
}
// Step 1: Copy in the path, making sure it's terminated.
// Step 2: Remove file from file system and response to user-level process.

void serve_dirty(u_int envid, struct Fsreq_dirty *rq) {

        // Your code here
        struct Open *pOpen;
        int r;

        if ((r = open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
                ipc_send(envid, r, 0, 0);
                return;
        }

        if ((r = file_dirty(pOpen->o_file, rq->req_offset)) < 0) {
                ipc_send(envid, r, 0, 0);
                return;
        }

        ipc_send(envid, 0, 0, 0);
}

void serve_sync(u_int envid) {
        fs_sync();
        ipc_send(envid, 0, 0, 0);
}

void serve(void) {
        u_int req, whom, perm;

        for (;;) {
                perm = 0;

                req = ipc_recv(&whom, REQVA, &perm);
#ifdef DEBUG
                writef("serve@serv.c: received new req from env %x\n", whom);
#endif

                // All requests must contain an argument page
                if (!(perm & PTE_V)) {
                        writef("Invalid request from %08x: no argument page\n", whom);
                        continue; // just leave it hanging, waiting for the next request.
                }

#ifdef DEBUG
                writef("serve@serv.c: calling corresponding functions\n");
#endif
                switch (req) {
                        case FSREQ_OPEN:
                                serve_open(whom, (struct Fsreq_open *)REQVA);
                                break;

                        case FSREQ_MAP:
                                serve_map(whom, (struct Fsreq_map *)REQVA);
                                break;

                        case FSREQ_SET_SIZE:
                                serve_set_size(whom, (struct Fsreq_set_size *)REQVA);
                                break;

                        case FSREQ_CLOSE:
                                serve_close(whom, (struct Fsreq_close *)REQVA);
                                break;

                        case FSREQ_DIRTY:
                                serve_dirty(whom, (struct Fsreq_dirty *)REQVA);
                                break;

                        case FSREQ_REMOVE:
                                serve_remove(whom, (struct Fsreq_remove *)REQVA);
                                break;

                        case FSREQ_SYNC:
                                serve_sync(whom);
                                break;

                        default:
                                writef("Invalid request code %d from %08x\n", whom, req);
                                break;
                }

                syscall_mem_unmap(0, REQVA);
        }
}

void umain(void) {
#ifdef DEBUG
        writef("umain@serv.c: file serve started\n");
#endif
        user_assert(sizeof(struct File) == BY2FILE);

        writef("FS is running\n");

        writef("FS can do I/O\n");

        serve_init();
        fs_init();
        fs_test();

#ifdef DEBUG
        writef("umain@serv.c: init succeeded, starting file serves\n");
#endif
        serve();
}


