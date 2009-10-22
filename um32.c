#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>

#define DEBUG 1

#define NUM_REGS 8

#if DEBUG
    #define DEBUG_LOG(msg, ...) fprintf(stderr, msg"\n", ## __VA_ARGS__)
#else
    #define DEBUG_LOG(msg, ...)
#endif

typedef uint32_t uint32;
typedef uint8_t uint8;

typedef struct {
    uint32 size;
    uint32 *mem;
} _array, *array;

typedef struct {
    uint32 regs[NUM_REGS];
    array *heap[1<<8];
    uint32 ip;
} _machine, *machine;

// translates a virtual platter address to a physical address
array v2p (uint32 addr, machine m)
{
    uint8 A = (addr & 0xFF000000) >> 24;
    uint8 B = (addr & 0x00FF0000) >> 16;
    uint8 C = (addr & 0x0000FF00) >> 8;
    uint8 D = (addr & 0x000000FF);

    array *Atbl = m->heap[A];
    if (Atbl != NULL) {
        array *Btbl = (array *)Atbl[B];
        if (Btbl != NULL) {
            array *Ctbl = (array *)Btbl[C];
            if (Ctbl != NULL) {
                return Ctbl[D];
            }
        }
    }

    return NULL; 
}

array allocMem (uint32 addr, uint32 size, machine m)
{
    uint8 A = (addr & 0xFF000000) >> 24;
    uint8 B = (addr & 0x00FF0000) >> 16;
    uint8 C = (addr & 0x0000FF00) >> 8;
    uint8 D = (addr & 0x000000FF);

    array *Atbl = m->heap[A];
    if (Atbl == NULL) {
        m->heap[A] = calloc(1<<8, sizeof(array));
        Atbl = (array *)m->heap[A];
    }

    array *Btbl = (array *)Atbl[B];
    if (Btbl == NULL) {
        Atbl[B] = calloc(1<<8, sizeof(array));
        Btbl = (array *)Atbl[B];
    }

    array *Ctbl = (array *)Btbl[C];
    if (Ctbl == NULL) {
        Btbl[C] = calloc(1<<8, sizeof(array));
        Ctbl = (array *)Btbl[C];
    }

    if (Ctbl[D] != NULL) {
        DEBUG_LOG("Attempt to alloc an array at %u when an array already exists", addr);
        return NULL;
    }
   
    array newArr = calloc(1, sizeof(_array));
    if (newArr == NULL) {
        DEBUG_LOG("Couldn't allocate %u bytes of memory at %u", size, addr);
    }
    
    Ctbl[D] = newArr;
    newArr->mem = calloc(size, sizeof(uint32));
    newArr->size = size;

    return newArr;
}

uint32 *readArray (uint32 addr, uint32 offset, machine m)
{
    array a = v2p(addr, m);
    if (a == NULL) {
        DEBUG_LOG("Array not found at address %u", addr);
        return NULL;
    }
    if (offset >= a->size) {
        DEBUG_LOG("Array index out of bounds: %u/%u", addr, offset);
        return NULL; 
    }
    return a->mem + offset;
}

uint32 *writeArray (uint32 addr, uint32 offset, uint32 value, machine m)
{
    array a = v2p(addr, m);
    if (a == NULL) {
        DEBUG_LOG("Array not found at address %u", addr);
        return NULL;
    }
    if (offset >= a->size) {
        DEBUG_LOG("Array index out of bounds: %u/%u", addr, offset);
        return NULL; 
    }
    *(a->mem + offset) = value;
    return a->mem + offset;
}

// returns the next instruction address, or 0 for the next instruction
uint32 execute (uint32 ins, machine m)
{
    return 0;
}

int runTest (void)
{
    machine m = (machine)calloc(1, sizeof(_machine));
    printf("Creating memory at location 0...\n");
    array a = allocMem(0, 10, m);
    if (a == NULL) {
        printf("Allocation failed!\n");
        free(m);
        return 1;
    }
    free(m);
    return 0;
}

void usage (void) 
{
    printf("Usage: %s file\n", getprogname());
}

static struct option longopts[] = {
    { "test",           no_argument,        NULL,   't' },
    { NULL,             0,                  NULL,    0  }
};

int main (int argc, char *argv[]) 
{
    if (argc < 2) {
        usage();
        return 0;
    }

    opterr = 0;
    int ch;
    int status;
    while ((ch = getopt_long(argc, argv, NULL, longopts, NULL)) != -1) {
        switch(ch) {
            case 't':
                status = runTest();
                fflush(stdout);
                exit(status);
                break;
            default:
                break;
        }
    }

    return 0;
}
