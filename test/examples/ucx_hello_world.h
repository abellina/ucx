/**
* Copyright (C) Mellanox Technologies Ltd. 2001-2016.  ALL RIGHTS RESERVED.
*
* See file LICENSE for terms.
*/

#ifndef UCX_HELLO_WORLD_H
#define UCX_HELLO_WORLD_H

#include <ucs/memory/memory_type.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>

#ifdef UCX_HELLO_WORLD_CUDA
# include <cuda.h>
# include <cuda_runtime.h>
#endif

#define STRUCT_FIELD_PTR(_struct, _type, _field)     \
    ((uint8_t*)(_struct) + offsetof(_type, _field))

#define CHKERR_ACTION(_cond, _msg, _action) \
    do { \
        if (_cond) { \
            fprintf(stderr, "Failed to %s\n", _msg); \
            _action; \
        } \
    } while (0)

#define CHKERR_JUMP(_cond, _msg, _label) \
    CHKERR_ACTION(_cond, _msg, goto _label)

#define CHKERR_JUMP_RETVAL(_cond, _msg, _label, _retval) \
    do { \
        if (_cond) { \
            fprintf(stderr, "Failed to %s, return value %d\n", _msg, _retval); \
            goto _label; \
        } \
    } while (0)


#define CUDA_FUNC(_func)                                   \
    do {                                                   \
        cudaError_t _result = (_func);                     \
        if (cudaSuccess != _result) {                      \
            fprintf(stderr, "%s failed: %s",               \
                    #_func, cudaGetErrorString(_result));  \
        }                                                  \
    } while(0)


struct mem_type_allocator {
    void*   (*malloc)(size_t length);
    void    (*free)(void *address);
    void*   (*memcpy)(void *dst, const void *src, size_t count);
    void*   (*memset)(void *dst, int value, size_t count);            
};

static ucs_memory_type_t test_mem_type = UCS_MEMORY_TYPE_HOST;

#ifdef UCX_HELLO_WORLD_CUDA
void *cuda_malloc(size_t length)
{
    void *ptr;
    CUDA_FUNC(cudaMalloc(&ptr, length));
    return ptr;
}

void *cuda_malloc_managed(size_t length)
{
    void *ptr;
    CUDA_FUNC(cudaMallocManaged(&ptr, length, cudaMemAttachGlobal));
    return ptr;
}

void cuda_free(void *address)
{
    cudaFree(address);
}

void *cuda_memcpy(void *dst, const void *src, size_t count)
{
    cudaMemcpy(dst, src, count, cudaMemcpyDefault);
    return dst;
}

void *cuda_memset(void *dst, int value, size_t count)
{
    cudaMemset(dst, value, count);
    return dst;
}
#endif

static struct mem_type_allocator mem_type_allocators[UCS_MEMORY_TYPE_LAST] = {
    {
        malloc, free, memcpy, memset
    },
#ifdef UCX_HELLO_WORLD_CUDA
    {
        cuda_malloc, cuda_free, cuda_memcpy, cuda_memset
    },
    {
        cuda_malloc_managed, cuda_free, cuda_memcpy, cuda_memset
    }
#endif
};

int server_connect(uint16_t server_port)
{
    struct sockaddr_in inaddr;
    int lsock  = -1;
    int dsock  = -1;
    int optval = 1;
    int ret;

    lsock = socket(AF_INET, SOCK_STREAM, 0);
    CHKERR_JUMP(lsock < 0, "open server socket", err);

    optval = 1;
    ret = setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    CHKERR_JUMP(ret < 0, "server setsockopt()", err_sock);

    inaddr.sin_family      = AF_INET;
    inaddr.sin_port        = htons(server_port);
    inaddr.sin_addr.s_addr = INADDR_ANY;
    memset(inaddr.sin_zero, 0, sizeof(inaddr.sin_zero));
    ret = bind(lsock, (struct sockaddr*)&inaddr, sizeof(inaddr));
    CHKERR_JUMP(ret < 0, "bind server", err_sock);

    ret = listen(lsock, 0);
    CHKERR_JUMP(ret < 0, "listen server", err_sock);

    fprintf(stdout, "Waiting for connection...\n");

    /* Accept next connection */
    dsock = accept(lsock, NULL, NULL);
    CHKERR_JUMP(dsock < 0, "accept server", err_sock);

    close(lsock);

    return dsock;

err_sock:
    close(lsock);

err:
    return -1;
}

int client_connect(const char *server, uint16_t server_port)
{
    struct sockaddr_in conn_addr;
    struct hostent *he;
    int connfd;
    int ret;

    connfd = socket(AF_INET, SOCK_STREAM, 0);
    CHKERR_JUMP(connfd < 0, "open client socket", err);

    he = gethostbyname(server);
    CHKERR_JUMP((he == NULL || he->h_addr_list == NULL), "found a host", err_conn);

    conn_addr.sin_family = he->h_addrtype;
    conn_addr.sin_port   = htons(server_port);

    memcpy(&conn_addr.sin_addr, he->h_addr_list[0], he->h_length);
    memset(conn_addr.sin_zero, 0, sizeof(conn_addr.sin_zero));

    ret = connect(connfd, (struct sockaddr*)&conn_addr, sizeof(conn_addr));
    CHKERR_JUMP(ret < 0, "connect client", err_conn);

    return connfd;

err_conn:
    close(connfd);
err:
    return -1;
}

static int barrier(int oob_sock)
{
    int dummy = 0;
    ssize_t res;

    res = send(oob_sock, &dummy, sizeof(dummy), 0);
    if (res < 0) {
        return res;
    }

    res = recv(oob_sock, &dummy, sizeof(dummy), MSG_WAITALL);

    /* number of received bytes should be the same as sent */
    return !(res == sizeof(dummy));
}

static void generate_test_string(char *str, int size)
{
    char val;
    int i;

    for (i = 0; i < (size - 1); ++i) {
        val    = 'A' + (i % 26);
        mem_type_allocators[test_mem_type].memcpy(&str[i],
                                                  &val,
                                                  sizeof(val));
    }
    val = '\0';
    mem_type_allocators[test_mem_type].memcpy(&str[i],
                                              &val,
                                              sizeof(val));
}

#endif /* UCX_HELLO_WORLD_H */
