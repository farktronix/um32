#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <arpa/inet.h>

#define DEBUG 1
#define TRACE 1

#define NUM_REGS 8

#if DEBUG
    #define DEBUG_LOG(msg, ...) do { if (!sUnitTesting) fprintf(stderr, msg"\n", ## __VA_ARGS__); } while (0)
#else
    #define DEBUG_LOG(msg, ...)
#endif

#if TRACE 
    #define INS_TRACE(msg, ...) do { if (!sUnitTesting) fprintf(stderr, msg"\n", ## __VA_ARGS__); } while (0)
#else
    #define INS_TRACE(msg, ...)
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
    uint32 ip;
    array *heap[1<<8];
    array prog;
    uint32 lastAddr;
} _machine, *machine;


// ----------------------------------------------------------------------------
// Memory Management

// translates a virtual platter address to a physical address
array v2p (machine m, uint32 addr)
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

array allocMem (machine m, uint32 addr, uint32 size)
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

uint32 allocArray (machine m, uint32 size)
{
    array dest = NULL;
    do {
        dest = v2p(m, ++(m->lastAddr));
        if (dest == NULL) break;
    } while (1);
    allocMem(m, m->lastAddr, size);
    return m->lastAddr;
}

// returns 1 if the array didn't previously exist, 0 if it was successfully freed
int abandonArray (machine m, uint32 addr)
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

array dupArray (machine m, uint32 sourceAddr, uint32 destAddr)
{
    array source = v2p(m, sourceAddr);
    if (sourceAddr == destAddr) return source;
    abandonArray(m, destAddr);
    array dest = allocMem(m, destAddr, source->size);
    memcpy(dest->mem, source->mem, source->size * sizeof(uint32));
    return dest;
}

uint32 *memAtArray (machine m, uint32 addr, uint32 offset)
{
    array a = v2p(m, addr);
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

uint32 readArray (machine m, uint32 addr, uint32 offset, char *success)
{
    uint32 *retval = memAtArray(m, addr, offset);
    if (retval == NULL) {
        if (success) *success = 0;
        return (uint32)-1;
    } else {
        if (success) *success = 1;
        return *retval;
    }
}

uint32 *writeArray (machine m, uint32 addr, uint32 offset, uint32 value)
{
    uint32 *retval = memAtArray(m, addr, offset);
    if (retval == NULL) return NULL;
    *retval = value;
    return retval;
}



// ----------------------------------------------------------------------------
// Instruction Execution

// returns the next instruction address, or 0 for the next instruction
int execute (machine m, uint32 ins)
{
    int rc = 0;

    uint32 opcode = (ins & 0xF0000000) >> 28;
    uint32 regA = (ins & 0x000001C0) >> 6;
    uint32 regB = (ins & 0x00000038) >> 3;
    uint32 regC = (ins & 0x00000007);

    INS_TRACE("opcode: %d\tA: %x\tB: %x\tC: %x", opcode, regA, regB, regC);

    int input = 0;

//                          A     C
//                          |     |
//                          vvv   vvv                    
//  .--------------------------------.
//  |VUTSRQPONMLKJIHGFEDCBA9876543210|
//  `--------------------------------'
//   ^^^^                      ^^^
//   |                         |
//   operator number           B
//
//  Figure 2. Standard Operators

    switch (opcode) {
        case 0:
            // Conditional Move.
            //  The register A receives the value in register B,
            //  unless the register C contains 0.
            INS_TRACE("Conditional Move: r%u = %x if %x", regA, m->regs[regB], m->regs[regC]);
            if (m->regs[regC] != 0) {
                m->regs[regA] = m->regs[regB];
            }
            break;

        case 1:
            // Array Index.
            //   The register A receives the value stored at offset
            //   in register C in the array identified by B.
            INS_TRACE("Array Index: r%d = %x:%x", regA, m->regs[regB], m->regs[regC]);
            m->regs[regA] = readArray(m, m->regs[regB], m->regs[regC], NULL);
            break;

        case 2:
            // Array Amendment.
            //   The array identified by A is amended at the offset
            //   in register B to store the value in register C.
            INS_TRACE("Array Amendment: %x:%x = %x", m->regs[regA], m->regs[regB], m->regs[regC]);
            writeArray(m, m->regs[regA], m->regs[regB], m->regs[regC]);
            break;

        case 3:
            // Addition.
            //   The register A receives the value in register B plus
            //   the value in register C, modulo 2^32.
            INS_TRACE("Addition: r%d = %u + %u", regA, m->regs[regB], m->regs[regC]);
            m->regs[regA] = (m->regs[regB] + m->regs[regC]);
            break;

        case 4:
            // Multiplication.
            //   The register A receives the value in register B times
            //   the value in register C, modulo 2^32.
            INS_TRACE("Multiplication: r%d = %u * %u", regA, m->regs[regB], m->regs[regC]);
            m->regs[regA] = (m->regs[regB] * m->regs[regC]);
            break;

        case 5:
            // Division.
            //   The register A receives the value in register B
            //   divided by the value in register C, if any, where
            //   each quantity is treated treated as an unsigned 32
            //   bit number.
            INS_TRACE("Division: r%d = %u / %u", regA, m->regs[regB], m->regs[regC]);
            m->regs[regA] = (m->regs[regB] / m->regs[regC]);
            break;

        case 6:
            // Not-And.
            //   Each bit in the register A receives the 1 bit if
            //   either register B or register C has a 0 bit in that
            //   position.  Otherwise the bit in register A receives
            //   the 0 bit.
            INS_TRACE("Not-And: r%d = %x ~& %x", regA, m->regs[regB], m->regs[regC]);
            m->regs[regA] = ~(m->regs[regB] & m->regs[regC]);
            break;

        case 7:
            // Halt.
            //   The universal machine stops computation.
            INS_TRACE("Halt");
            printf("\n\n\nHALT\n");
            rc = 1;
            break;

        case 8:
            // Allocation.
            //   A new array is created with a capacity of platters
            //   commensurate to the value in the register C. This
            //   new array is initialized entirely with platters
            //   holding the value 0. A bit pattern not consisting of
            //   exclusively the 0 bit, and that identifies no other
            //   active allocated array, is placed in the B register.
            INS_TRACE("Allocation: %u", m->regs[regC]);
            m->regs[regB] = allocArray(m, m->regs[regC]);
            break;

        case 9:
            // Abandonment.
            //   The array identified by the register C is abandoned.
            //   Future allocations may then reuse that identifier.
            INS_TRACE("Abandonment: %x", m->regs[regC]);
            abandonArray(m, m->regs[regC]);
            break;

        case 10:
            // Output.
            //   The value in the register C is displayed on the console
            //   immediately. Only values between and including 0 and 255
            //   are allowed.
            INS_TRACE("Output: %c", (char)m->regs[regC]);
            putchar(m->regs[regC]);
            break;

        case 11:
            // Input.
            //   The universal machine waits for input on the console.
            //   When input arrives, the register C is loaded with the
            //   input, which must be between and including 0 and 255
            //   If the end of input has been signaled, then the
            //   register C is endowed with a uniform value pattern
            //   where every place is pregnant with the 1 bit.
            INS_TRACE("Input");
            input = getc(stdin);
            if (input == EOF) {
                m->regs[regC] = 0xFFFFFFFF;
            } else {
                m->regs[regC] = input;
            }
            break;

        case 12:
            // Load Program.
            //   The array identified by the B register is duplicated
            //   and the duplicate shall replace the '0' array,
            //   regardless of size. The execution finger is placed
            //   to indicate the platter of this array that is
            //   described by the offset given in C, where the value
            //   0 denotes the first platter, 1 the second, et
            //   cetera.
            //
            //   The '0' array shall be the most sublime choice for
            //   loading, and shall be handled with the utmost
            //   velocity.
            INS_TRACE("Load Program: %x", m->regs[regB]);
            m->prog = dupArray(m, m->regs[regB], 0);
            break;

        case 13:
            // Orthography.
            //
            //       A  
            //       |  
            //       vvv
            //  .--------------------------------.
            //  |VUTSRQPONMLKJIHGFEDCBA9876543210|
            //  `--------------------------------'
            //   ^^^^   ^^^^^^^^^^^^^^^^^^^^^^^^^
            //   |      |
            //   |      value
            //   |
            //   operator number
            //
            //   The value indicated is loaded into the register A
            //   forthwith.
            regA = (ins & 0x0E000000) >> 25;
            INS_TRACE("Orthography: r%u = 0x%08x", regA, ins & 0x1FFFFFF);
            m->regs[regA] = ins & 0x1FFFFFF;
            break;

        default:
            fprintf(stderr, "Illegal instruction encountered! (%d)\n", opcode);
            return 1;
    }

    return rc;
}

void runMachine (machine m)
{
    int rc = 0;
    uint32 ins = 0;

    DEBUG_LOG("Firing up the virtual machine...");

    while (rc == 0) {
        ins = htonl(m->prog->mem[m->ip]);
        INS_TRACE("Executing instruction at address %u: 0x%04x", m->ip, ins);
        rc = execute(m, ins);
        m->ip++;
        INS_TRACE("Registers: r0:0x%04x r1:0x%04x r2:0x%04x r3:0x%04x r4:0x%04x r5:0x%04x r6:0x%04x r7:0x%04x", m->regs[0], m->regs[1], m->regs[2], m->regs[3], m->regs[4], m->regs[5], m->regs[6], m->regs[7]);
        INS_TRACE("");
    }
}



// ----------------------------------------------------------------------------
// Unit Tests

uint32 testRead (machine m, uint32 addr, uint32 offset, uint32 expected)
{
    char success = 0;
    uint32 mem = readArray(m, addr, offset, &success);
    if (success == 1) {
        if (mem != expected) {
            printf("ERROR: Memory read at %u:%u (0x%x) was not what was expected (0x%x).\n", addr, offset, mem, expected);
        }
    } else {
        printf("ERROR: Failed to read memory at location %u:%u\n", addr, offset);
    }
    return mem;
}

uint32 testWrite (machine m, uint32 addr, uint32 offset, uint32 value)
{
    uint32 *result = writeArray(m, addr, offset, value);
    if (result == NULL) {
        printf("ERROR: Write failed at %u:%u\n", addr, offset);
        return 0;
    }
    if (*result != value) {
        printf("ERROR: Write (0x%x) was not what was expected (0x%x).\n", *result, value);
    }
    return *result;
}

int memTests (void) 
{
    int rc = 0;
    int mem = 0;
    char success = 0;

    machine m = (machine)calloc(1, sizeof(_machine));
    printf("Testing array allocations.\n");
    array a = allocMem(m, 0, 10);
    if (a == NULL) {
        printf("Allocation failed!\n");
        free(m);
        return 1;
    }
    a = allocMem(m, 0, 10);
    if (a != NULL) {
        printf("ERROR: Double array allocation failed\n");
    }
    uint32 loc = 0xFFFFFFFE;
    a = allocMem(m, loc, 10);
    if (a == NULL) {
        printf("ERROR: Could not allocate memory at 0x%x\n", loc);
    }

    printf("Testing array read/write.\n");
    testRead(m, 0, 0, 0);
    testWrite(m, 0, 0, 0xDEADBEEF);
    testRead(m, 0, 0, 0xDEADBEEF);
    testWrite(m, loc, 9, 0xDEADBEEF);
    testRead(m, loc, 0, 0);
    testRead(m, loc, 9, 0xDEADBEEF);


    printf("Testing array abandons.\n");
    rc = abandonArray(m, 0);
    if (rc != 0) {
        printf("ERROR: Could not abandon array 0\n");
    }
    rc = abandonArray(m, 0);
    if (rc == 0) {
        printf("ERROR: Double abandon shouldn't succeed\n");
    }
    mem = readArray(m, 0, 0, &success);
    if (success) {
        printf("ERROR: Read from a memory location that was abandonded\n");
    }

    free(m);
    return 0;
}

int runTest (void)
{
    sUnitTesting = 1;
    int rc = 0;
    rc |= memTests();
    return rc;
}



// ----------------------------------------------------------------------------
// Main

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
                if (status == 0) printf("All tests passed.\n");
                fflush(stdout);
                exit(status);
                break;
            default:
                break;
        }
    }
    
    machine m = (machine)calloc(1, sizeof(_machine));
    char *progname = argv[1];
    int fd = open(progname, O_RDONLY);
    if (fd == -1) {
        printf("Could not open file at %s: %s\n", progname, strerror(errno));
        exit(1);
    }
    struct stat sb = {0};
    fstat(fd, &sb);
    m->prog = allocMem(m, 0, sb.st_size / sizeof(uint32));
    DEBUG_LOG("Loading %llu bytes of program into memory (%llu instructions)", sb.st_size, sb.st_size/sizeof(uint32));
   
    uint32 buf = 0;
    uint32 ip = 0;
    while (read(fd, &buf, 4) > 0) {
        m->prog->mem[ip++] = buf;
    }
    close(fd);

    runMachine(m);

    return 0;
}
