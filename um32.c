#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>

#define DEBUG 1

#define NUM_REGS 8

#if DEBUG
    #define DEBUG_LOG(msg, ...) do { if (!sUnitTesting) fprintf(stderr, msg"\n", ## __VA_ARGS__); } while (0)
#else
    #define DEBUG_LOG(msg, ...)
#endif

static char sUnitTesting = 0;

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

// returns 1 if the array didn't previously exist, 0 if it was successfully freed
int abandonArray (uint32 addr, machine m)
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
                array a = Ctbl[D];
                if (a != NULL) {
                    free(a->mem);
                    free(a);
                    Ctbl[D] = NULL;
                    return 0;
                }
            }
        }
    }

    return 1;
}

uint32 *memAtArray (uint32 addr, uint32 offset, machine m)
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

uint32 readArray (uint32 addr, uint32 offset, machine m, char *success)
{
    uint32 *retval = memAtArray(addr, offset, m);
    if (retval == NULL) {
        if (success) *success = 0;
        return (uint32)-1;
    } else {
        if (success) *success = 1;
        return *retval;
    }
}

uint32 *writeArray (uint32 addr, uint32 offset, uint32 value, machine m)
{
    uint32 *retval = memAtArray(addr, offset, m);
    if (retval == NULL) return NULL;
    *retval = value;
    return retval;
}

// returns the next instruction address, or 0 for the next instruction
uint32 execute (uint32 ins, machine m)
{
    return 0;
}

uint32 testRead (uint32 addr, uint32 offset, machine m)
{
    char success = 0;
    int mem = readArray(addr, offset, m, &success);
    if (success == 1) {
        printf("Reading memory at location %u:%u is: 0x%x\n", addr, offset, mem);
    } else {
        printf("Failed to read memory at location %u:%u\n", addr, offset);
    }
    return mem;
}

int runTest (void)
{
    sUnitTesting = 1;
    uint32 *result = NULL;
    int rc = 0;
    int mem = 0;
    char success = 0;

    machine m = (machine)calloc(1, sizeof(_machine));
    printf("Creating memory at location 0...\n");
    array a = allocMem(0, 10, m);
    if (a == NULL) {
        printf("Allocation failed!\n");
        free(m);
        return 1;
    }

    testRead(0, 0, m);
    result = writeArray(0, 0, 0xDEADBEEF, m);
    testRead(0, 0, m);

    a = allocMem(0, 10, m);
    if (a != NULL) {
        printf("ERROR: Double array allocation failed\n");
    }

    rc = abandonArray(0, m);
    if (rc != 0) {
        printf("ERROR: Could not abandon array 0\n");
    }
    rc = abandonArray(0, m);
    if (rc == 0) {
        printf("ERROR: Double abandon shouldn't succeed\n");
    }

    mem = readArray(0, 0, m, &success);
    if (success) {
        printf("ERROR: Read from a memory location that was abandonded\n");
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
    while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
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
