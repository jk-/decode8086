#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

typedef unsigned int uint;
typedef uint8_t      uint8;
typedef uint16_t     uint16;
typedef uint32_t     uint32;
typedef int8_t       int8;
typedef int16_t      int16;

#define MASK_W    0b001
#define MASK_D    0b010
#define MASK_S    0b100
#define MASK_V    0b1000
#define MASK_ES  (0b00 << 4)
#define MASK_CS  (0b01 << 4)
#define MASK_SS  (0b10 << 4)
#define MASK_DS  (0b11 << 4)
#define MASK_MO  (0b1  << 6) // memory only
#define MASK_LB  (0b1  << 7) // has label

#define MASK_MOD  0b011
#define MASK_RM   0b111
#define MASK_REG  0b111

#define MODE_MEM0  0b00
#define MODE_MEM8  0b01
#define MODE_MEM16 0b10
#define MODE_REG   0b11


// implicit prefixes
#define PFX_WIDE     (0b1  << 0)
#define PFX_FAR      (0b1  << 1)
// explicit prefixes
#define PFX_LOCK     (0b1  << 2)
#define PFX_SGMNT    (0b1  << 3)
#define PFX_SGMNT_ES (0b00 << 4)
#define PFX_SGMNT_CS (0b01 << 4)
#define PFX_SGMNT_SS (0b10 << 4)
#define PFX_SGMNT_DS (0b11 << 4)
#define PFX_REP      (0b1  << 6)
#define PFX_REPNE    (0b1  << 7)

#define SGMNT_OP(prefixes) (((prefixes) >> 4) & 0b11)

#define MOD(byte)  ((byte >> 6) & MASK_MOD)
#define RM(byte)   ((byte >> 0) & MASK_RM)
#define REG(byte)  ((byte >> 3) & MASK_REG)
#define REG2(byte) ((byte >> 0) & MASK_REG)
#define ESC1(byte) (((byte) >> 0) & 0b111)
#define ESC2(byte) (((byte) >> 3) & 0b111)
#define EXTD(byte) (((byte) >> 3) & 0b111)

#define FIELD_MOD(fields)  ((fields >> 0) & MASK_MOD)
#define FIELD_RM(fields)   ((fields >> 4) & MASK_RM)
#define FIELD_REG(fields)  ((fields >> 7) & MASK_REG)

#define W(flags) (!!(flags & MASK_W))

static char *regs[2][8] = {
    { "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh" },
    { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di" }
};


typedef enum {
    NONE,

    // [mod ... r/m] [disp-lo] [disp-hi]
    RM,
    // [mod ... r/m] [disp-lo] [disp-hi] (store 1/cl)
    RM_V,
    // [mod 0 sr r/m] [disp-lo] [disp-hi]
    RM_SR,
    // [mod reg r/m] [disp-lo] [disp-hi]
    RM_REG,
    // [mod ... r/m] [disp-lo] [disp-hi] [data]
    RM_IMM,
    // [... xxx] [mod yyy r/m] [disp-lo] [disp-hi]
    RM_ESC,

    ACC_DX,
    // [data-8]
    ACC_IMM8,
    // [data-lo] [data-hi]
    ACC_IMM,
    // [... reg]
    ACC_REG,
    // [addr-lo] [addr-hi]
    ACC_MEM,

    // [... reg]
    REG,
    // [...reg] [data-hi] [data-lo]
    REG_IMM,

    // [... sr ...]
    SR,

    // [data-lo] [data-hi]
    IMM,

    // [ip-inc-8]
    JMP_SHORT,
    // [ip-inc-lo] [ip-inc-hi]
    JMP_NEAR,
    // [ip-lo] [ip-hi] [cs-lo] [cs-hi]
    JMP_FAR,
 } FORMAT;

typedef enum {
    UNKNOWN = 0,

    AAA,    AAD,   AAM,   AAS,
    ADC,    ADD,   AND,   CALL,
    CALLF,  CBW,   CLC,   CLD,
    CLI,    CMC,   CMP,   CMPSB,
    CMPSW,  CWD,   DAA,   DAS,
    DEC,    DIV,   ESC,   HLT,
    IDIV,   IMUL,  IN,    INC,
    INT,    INT3,  INTO,  IRET,
    JA,     JAE,   JB,    JBE,
    JCXZ,   JE,    JG,    JGE,
    JL,     JLE,   JMP,   JMPF,
    JNE,    JNO,   JNS,   JO,
    JP,     JPO,   JS,    LAHF,
    LDS,    LEA,   LES,   LOCK,
    LODSB,  LODSW, LOOP,  LOOPZ,
    LOOPNZ, MOV,   MOVSB, MOVSW,
    MUL,    NEG,   NOP,   NOT,
    OR,     OUT,   POP,   POPF,
    PUSH,   PUSHF, RCL,   RCR,
    REP,    REPNE, RET,   RETF,
    ROL,    ROR,   SAHF,  SAR,
    SBB,    SCASB, SCASW, SGMNT,
    SHL,    SHR,   STC,   STD,
    STOSB,  STOSW, STI,   SUB,
    TEST,   WAIT,  XCHG,  XLAT,
    XOR,

    EXTD,
} TYPE;


typedef struct {
    TYPE   type;
    FORMAT format;
    uint8  flags;
    uint8  prefixes;
    uint8  size;
} InstructionData;

typedef struct {
    InstructionData structure;
    uint16          data;
    uint16          displacement;
    uint16          fields;
    uint            offset;
} Instruction;


InstructionData instruction_table[256] = {
	{ ADD,    RM_REG,    0,            0, 2 }, // 0x00
	{ ADD,    RM_REG,    MASK_W,          0, 2 }, // 0x01
	{ ADD,    RM_REG,    MASK_D,          0, 2 }, // 0x02
	{ ADD,    RM_REG,    MASK_D|MASK_W,      0, 2 }, // 0x03
	{ ADD,    ACC_IMM,   0,            0, 2 }, // 0x04
	{ ADD,    ACC_IMM,   MASK_W,          0, 3 }, // 0x05
	{ PUSH,   SR,        MASK_ES,         0, 1 }, // 0x06
	{ POP,    SR,        MASK_ES,         0, 1 }, // 0x07
	{ OR,     RM_REG,    0,            0, 2 }, // 0x08
	{ OR,     RM_REG,    MASK_W,          0, 2 }, // 0x09
	{ OR,     RM_REG,    MASK_D,          0, 2 }, // 0x0A
	{ OR,     RM_REG,    MASK_D|MASK_W,      0, 2 }, // 0x0B
	{ OR,     ACC_IMM,   0,            0, 2 }, // 0x0C
	{ OR,     ACC_IMM,   MASK_W,          0, 3 }, // 0x0D
	{ PUSH,   SR,        MASK_CS,         0, 1 }, // 0x0E
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0x0F
	{ ADC,    RM_REG,    0,            0, 2 }, // 0x10
	{ ADC,    RM_REG,    MASK_W,          0, 2 }, // 0x11
	{ ADC,    RM_REG,    MASK_D,          0, 2 }, // 0x12
	{ ADC,    RM_REG,    MASK_D|MASK_W,      0, 2 }, // 0x13
	{ ADC,    ACC_IMM,   0,            0, 2 }, // 0x14
	{ ADC,    ACC_IMM,   MASK_W,          0, 3 }, // 0x15
	{ PUSH,   SR,        MASK_SS,         0, 1 }, // 0x16
	{ POP,    SR,        MASK_SS,         0, 1 }, // 0x17
	{ SBB,    RM_REG,    0,            0, 2 }, // 0x18
	{ SBB,    RM_REG,    MASK_W,          0, 2 }, // 0x19
	{ SBB,    RM_REG,    MASK_D,          0, 2 }, // 0x1A
	{ SBB,    RM_REG,    MASK_D|MASK_W,      0, 2 }, // 0x1B
	{ SBB,    ACC_IMM,   0,            0, 2 }, // 0x1C
	{ SBB,    ACC_IMM,   MASK_W,          0, 3 }, // 0x1D
	{ PUSH,   SR,        MASK_DS,         0, 1 }, // 0x1E
	{ POP,    SR,        MASK_DS,         0, 1 }, // 0x1F
	{ AND,    RM_REG,    0,            0, 2 }, // 0x20
	{ AND,    RM_REG,    MASK_W,          0, 2 }, // 0x21
	{ AND,    RM_REG,    MASK_D,          0, 2 }, // 0x22
	{ AND,    RM_REG,    MASK_D|MASK_W,      0, 2 }, // 0x23
	{ AND,    ACC_IMM,   0,            0, 2 }, // 0x24
	{ AND,    ACC_IMM,   MASK_W,          0, 3 }, // 0x25
	{ SGMNT,  NONE,      MASK_ES,         0, 1 }, // 0x26
	{ DAA,    NONE,      0,            0, 1 }, // 0x27
	{ SUB,    RM_REG,    0,            0, 2 }, // 0x28
	{ SUB,    RM_REG,    MASK_W,          0, 2 }, // 0x29
	{ SUB,    RM_REG,    MASK_D,          0, 2 }, // 0x2A
	{ SUB,    RM_REG,    MASK_D|MASK_W,      0, 2 }, // 0x2B
	{ SUB,    ACC_IMM,   0,            0, 2 }, // 0x2C
	{ SUB,    ACC_IMM,   MASK_W,          0, 3 }, // 0x2D
	{ SGMNT,  NONE,      MASK_CS,         0, 1 }, // 0x2E
	{ DAS,    NONE,      0,            0, 1 }, // 0x2F
	{ XOR,    RM_REG,    0,            0, 2 }, // 0x30
	{ XOR,    RM_REG,    MASK_W,          0, 2 }, // 0x31
	{ XOR,    RM_REG,    MASK_D,          0, 2 }, // 0x32
	{ XOR,    RM_REG,    MASK_D|MASK_W,      0, 2 }, // 0x33
	{ XOR,    ACC_IMM,   0,            0, 2 }, // 0x34
	{ XOR,    ACC_IMM,   MASK_W,          0, 3 }, // 0x35
	{ SGMNT,  NONE,      MASK_SS,         0, 1 }, // 0x36
	{ AAA,    NONE,      0,            0, 1 }, // 0x37
	{ CMP,    RM_REG,    0,            0, 2 }, // 0x38
	{ CMP,    RM_REG,    MASK_W,          0, 2 }, // 0x39
	{ CMP,    RM_REG,    MASK_D,          0, 2 }, // 0x3A
	{ CMP,    RM_REG,    MASK_D|MASK_W,      0, 2 }, // 0x3B
	{ CMP,    ACC_IMM,   0,            0, 2 }, // 0x3C
	{ CMP,    ACC_IMM,   MASK_W,          0, 3 }, // 0x3D
	{ SGMNT,  NONE,      MASK_DS,         0, 1 }, // 0x3E
	{ AAS,    NONE,      0,            0, 1 }, // 0x3F
	{ INC,    REG,       MASK_W,          0, 1 }, // 0x40
	{ INC,    REG,       MASK_W,          0, 1 }, // 0x41
	{ INC,    REG,       MASK_W,          0, 1 }, // 0x42
	{ INC,    REG,       MASK_W,          0, 1 }, // 0x43
	{ INC,    REG,       MASK_W,          0, 1 }, // 0x44
	{ INC,    REG,       MASK_W,          0, 1 }, // 0x45
	{ INC,    REG,       MASK_W,          0, 1 }, // 0x46
	{ INC,    REG,       MASK_W,          0, 1 }, // 0x47
	{ DEC,    REG,       MASK_W,          0, 1 }, // 0x48
	{ DEC,    REG,       MASK_W,          0, 1 }, // 0x49
	{ DEC,    REG,       MASK_W,          0, 1 }, // 0x4A
	{ DEC,    REG,       MASK_W,          0, 1 }, // 0x4B
	{ DEC,    REG,       MASK_W,          0, 1 }, // 0x4C
	{ DEC,    REG,       MASK_W,          0, 1 }, // 0x4D
	{ DEC,    REG,       MASK_W,          0, 1 }, // 0x4E
	{ DEC,    REG,       MASK_W,          0, 1 }, // 0x4F
	{ PUSH,   REG,       MASK_W,          0, 1 }, // 0x50
	{ PUSH,   REG,       MASK_W,          0, 1 }, // 0x51
	{ PUSH,   REG,       MASK_W,          0, 1 }, // 0x52
	{ PUSH,   REG,       MASK_W,          0, 1 }, // 0x53
	{ PUSH,   REG,       MASK_W,          0, 1 }, // 0x54
	{ PUSH,   REG,       MASK_W,          0, 1 }, // 0x55
	{ PUSH,   REG,       MASK_W,          0, 1 }, // 0x56
	{ PUSH,   REG,       MASK_W,          0, 1 }, // 0x57
	{ POP,    REG,       MASK_W,          0, 1 }, // 0x58
	{ POP,    REG,       MASK_W,          0, 1 }, // 0x59
	{ POP,    REG,       MASK_W,          0, 1 }, // 0x5A
	{ POP,    REG,       MASK_W,          0, 1 }, // 0x5B
	{ POP,    REG,       MASK_W,          0, 1 }, // 0x5C
	{ POP,    REG,       MASK_W,          0, 1 }, // 0x5D
	{ POP,    REG,       MASK_W,          0, 1 }, // 0x5E
	{ POP,    REG,       MASK_W,          0, 1 }, // 0x5F
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0x60
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0x61
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0x62
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0x63
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0x64
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0x65
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0x66
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0x67
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0x68
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0x69
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0x6A
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0x6B
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0x6C
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0x6D
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0x6E
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0x6F
	{ JO,     JMP_SHORT, 0,            0, 2 }, // 0x70
	{ JNO,    JMP_SHORT, 0,            0, 2 }, // 0x71
	{ JB,     JMP_SHORT, 0,            0, 2 }, // 0x72
	{ JAE,    JMP_SHORT, 0,            0, 2 }, // 0x73
	{ JE,     JMP_SHORT, 0,            0, 2 }, // 0x74
	{ JNE,    JMP_SHORT, 0,            0, 2 }, // 0x75
	{ JBE,    JMP_SHORT, 0,            0, 2 }, // 0x76
	{ JA,     JMP_SHORT, 0,            0, 2 }, // 0x77
	{ JS,     JMP_SHORT, 0,            0, 2 }, // 0x78
	{ JNS,    JMP_SHORT, 0,            0, 2 }, // 0x79
	{ JP,     JMP_SHORT, 0,            0, 2 }, // 0x7A
	{ JPO,    JMP_SHORT, 0,            0, 2 }, // 0x7B
	{ JL,     JMP_SHORT, 0,            0, 2 }, // 0x7C
	{ JGE,    JMP_SHORT, 0,            0, 2 }, // 0x7D
	{ JLE,    JMP_SHORT, 0,            0, 2 }, // 0x7E
	{ JG,     JMP_SHORT, 0,            0, 2 }, // 0x7F
	{ EXTD,   NONE,      0,            0, 0 }, // 0x80
	{ EXTD,   NONE,      0,            0, 0 }, // 0x81
	{ EXTD,   NONE,      0,            0, 0 }, // 0x82
	{ EXTD,   NONE,      0,            0, 0 }, // 0x83
	{ TEST,   RM_REG,    0,            0, 2 }, // 0x84
	{ TEST,   RM_REG,    MASK_W,          0, 2 }, // 0x85
	{ XCHG,   RM_REG,    MASK_D,          0, 2 }, // 0x86
	{ XCHG,   RM_REG,    MASK_D|MASK_W,      0, 2 }, // 0x87
	{ MOV,    RM_REG,    0,            0, 2 }, // 0x88
	{ MOV,    RM_REG,    MASK_W,          0, 2 }, // 0x89
	{ MOV,    RM_REG,    MASK_D,          0, 2 }, // 0x8A
	{ MOV,    RM_REG,    MASK_D|MASK_W,      0, 2 }, // 0x8B
	{ EXTD,   NONE,      0,            0, 0 }, // 0x8C
	{ LEA,    RM_REG,    MASK_D|MASK_W|MASK_MO, 0, 2 }, // 0x8D
	{ EXTD,   NONE,      0,            0, 0 }, // 0x8E
	{ EXTD,   NONE,      0,            0, 0 }, // 0x8F
	{ NOP,    NONE,      MASK_W,          0, 1 }, // 0x90
	{ XCHG,   ACC_REG,   MASK_W,          0, 1 }, // 0x91
	{ XCHG,   ACC_REG,   MASK_W,          0, 1 }, // 0x92
	{ XCHG,   ACC_REG,   MASK_W,          0, 1 }, // 0x93
	{ XCHG,   ACC_REG,   MASK_W,          0, 1 }, // 0x94
	{ XCHG,   ACC_REG,   MASK_W,          0, 1 }, // 0x95
	{ XCHG,   ACC_REG,   MASK_W,          0, 1 }, // 0x96
	{ XCHG,   ACC_REG,   MASK_W,          0, 1 }, // 0x97
	{ CBW,    NONE,      0,            0, 1 }, // 0x98
	{ CWD,    NONE,      0,            0, 1 }, // 0x99
	{ CALL,   JMP_FAR,   0,            0, 5 }, // 0x9A
	{ WAIT,   NONE,      0,            0, 1 }, // 0x9B
	{ PUSHF,  NONE,      0,            0, 1 }, // 0x9C
	{ POPF,   NONE,      0,            0, 1 }, // 0x9D
	{ SAHF,   NONE,      0,            0, 1 }, // 0x9E
	{ LAHF,   NONE,      0,            0, 1 }, // 0x9F
	{ MOV,    ACC_MEM,   MASK_MO,         0, 3 }, // 0xA0
	{ MOV,    ACC_MEM,   MASK_W|MASK_MO,     0, 3 }, // 0xA1
	{ MOV,    ACC_MEM,   MASK_D|MASK_MO,     0, 3 }, // 0xA2
	{ MOV,    ACC_MEM,   MASK_D|MASK_W|MASK_MO, 0, 3 }, // 0xA3
	{ MOVSB,  NONE,      0,            0, 1 }, // 0xA4
	{ MOVSW,  NONE,      MASK_W,          0, 1 }, // 0xA5
	{ CMPSB,  NONE,      0,            0, 1 }, // 0xA6
	{ CMPSW,  NONE,      MASK_W,          0, 1 }, // 0xA7
	{ TEST,   ACC_IMM,   0,            0, 2 }, // 0xA8
	{ TEST,   ACC_IMM,   MASK_W,          0, 3 }, // 0xA9
	{ STOSB,  NONE,      0,            0, 1 }, // 0xAA
	{ STOSW,  NONE,      0,            0, 1 }, // 0xAB
	{ LODSB,  NONE,      0,            0, 1 }, // 0xAC
	{ LODSW,  NONE,      0,            0, 1 }, // 0xAD
	{ SCASB,  NONE,      0,            0, 1 }, // 0xAE
	{ SCASW,  NONE,      0,            0, 1 }, // 0xAF
	{ MOV,    REG_IMM,   0,            0, 2 }, // 0xB0
	{ MOV,    REG_IMM,   0,            0, 2 }, // 0xB1
	{ MOV,    REG_IMM,   0,            0, 2 }, // 0xB2
	{ MOV,    REG_IMM,   0,            0, 2 }, // 0xB3
	{ MOV,    REG_IMM,   0,            0, 2 }, // 0xB4
	{ MOV,    REG_IMM,   0,            0, 2 }, // 0xB5
	{ MOV,    REG_IMM,   0,            0, 2 }, // 0xB6
	{ MOV,    REG_IMM,   0,            0, 2 }, // 0xB7
	{ MOV,    REG_IMM,   MASK_W,          0, 3 }, // 0xB8
	{ MOV,    REG_IMM,   MASK_W,          0, 3 }, // 0xB9
	{ MOV,    REG_IMM,   MASK_W,          0, 3 }, // 0xBA
	{ MOV,    REG_IMM,   MASK_W,          0, 3 }, // 0xBB
	{ MOV,    REG_IMM,   MASK_W,          0, 3 }, // 0xBC
	{ MOV,    REG_IMM,   MASK_W,          0, 3 }, // 0xBD
	{ MOV,    REG_IMM,   MASK_W,          0, 3 }, // 0xBE
	{ MOV,    REG_IMM,   MASK_W,          0, 3 }, // 0xBF
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0xC0
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0xC1
	{ RET,    IMM,       MASK_W,          0, 3 }, // 0xC2
	{ RET,    NONE,      0,            0, 1 }, // 0xC3
	{ LES,    RM_REG,    MASK_D|MASK_W|MASK_MO, 0, 2 }, // 0xC4
	{ LDS,    RM_REG,    MASK_D|MASK_W|MASK_MO, 0, 2 }, // 0xC5
	{ EXTD,   NONE,      0,            0, 0 }, // 0xC6
	{ EXTD,   NONE,      0,            0, 0 }, // 0xC7
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0xC8
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0xC9
	{ RETF,   IMM,       MASK_W,          0, 3 }, // 0xCA
	{ RETF,   NONE,      0,            0, 1 }, // 0xCB
	{ INT3,   NONE,      0,            0, 1 }, // 0xCC
	{ INT,    IMM,       0,            0, 2 }, // 0xCD
	{ INTO,   NONE,      0,            0, 1 }, // 0xCE
	{ IRET,   NONE,      0,            0, 1 }, // 0xCF
	{ EXTD,   NONE,      0,            0, 0 }, // 0xD0
	{ EXTD,   NONE,      0,            0, 0 }, // 0xD1
	{ EXTD,   NONE,      0,            0, 0 }, // 0xD2
	{ EXTD,   NONE,      0,            0, 0 }, // 0xD3
	{ AAM,    NONE,      0,            0, 2 }, // 0xD4
	{ AAD,    NONE,      0,            0, 2 }, // 0xD5
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0xD6
	{ XLAT,   NONE,      0,            0, 1 }, // 0xD7
	{ ESC,    RM_ESC,    MASK_W,          0, 2 }, // 0xD8
	{ ESC,    RM_ESC,    MASK_W,          0, 2 }, // 0xD9
	{ ESC,    RM_ESC,    MASK_W,          0, 2 }, // 0xDA
	{ ESC,    RM_ESC,    MASK_W,          0, 2 }, // 0xDB
	{ ESC,    RM_ESC,    MASK_W,          0, 2 }, // 0xDC
	{ ESC,    RM_ESC,    MASK_W,          0, 2 }, // 0xDD
	{ ESC,    RM_ESC,    MASK_W,          0, 2 }, // 0xDE
	{ ESC,    RM_ESC,    MASK_W,          0, 2 }, // 0xDF
	{ LOOPNZ, JMP_SHORT, 0,            0, 2 }, // 0xE0
	{ LOOPZ,  JMP_SHORT, 0,            0, 2 }, // 0xE1
	{ LOOP,   JMP_SHORT, 0,            0, 2 }, // 0xE2
	{ JCXZ,   JMP_SHORT, 0,            0, 2 }, // 0xE3
	{ IN,     ACC_IMM8,  0,            0, 2 }, // 0xE4
	{ IN,     ACC_IMM8,  MASK_W,          0, 2 }, // 0xE5
	{ OUT,    ACC_IMM8,  MASK_D,          0, 2 }, // 0xE6
	{ OUT,    ACC_IMM8,  MASK_D|MASK_W,      0, 2 }, // 0xE7
	{ CALL,   JMP_NEAR,  0,            0, 3 }, // 0xE8
	{ JMP,    JMP_NEAR,  0,            0, 3 }, // 0xE9
	{ JMP,    JMP_FAR,   0,            0, 5 }, // 0xEA
	{ JMP,    JMP_SHORT, 0,            0, 2 }, // 0xEB
	{ IN,     ACC_DX,    0,            0, 1 }, // 0xEC
	{ IN,     ACC_DX,    MASK_W,          0, 1 }, // 0xED
	{ OUT,    ACC_DX,    MASK_D,          0, 1 }, // 0xEE
	{ OUT,    ACC_DX,    MASK_D|MASK_W,      0, 1 }, // 0xEF
	{ LOCK,   NONE,      0,            0, 1 }, // 0xF0
	{ UNKNOWN,    NONE,      0,            0, 1 }, // 0xF1
	{ REPNE,  NONE,      0,            0, 1 }, // 0xF2
	{ REP,    NONE,      0,            0, 1 }, // 0xF3
	{ HLT,    NONE,      0,            0, 1 }, // 0xF4
	{ CMC,    NONE,      0,            0, 1 }, // 0xF5
	{ EXTD,   NONE,      0,            0, 0 }, // 0xF6
	{ EXTD,   NONE,      0,            0, 0 }, // 0xF7
	{ CLC,    NONE,      0,            0, 1 }, // 0xF8
	{ STC,    NONE,      0,            0, 1 }, // 0xF9
	{ CLI,    NONE,      0,            0, 1 }, // 0xFA
	{ STI,    NONE,      0,            0, 1 }, // 0xFB
	{ CLD,    NONE,      0,            0, 1 }, // 0xFC
	{ STD,    NONE,      0,            0, 1 }, // 0xFD
	{ EXTD,   NONE,      0,            0, 0 }, // 0xFE
	{ EXTD,   NONE,      0,            0, 0 }, // 0xFF
};

InstructionData instruction_table_extd[17][8] = {
    // [0x00]: 0x80 (0b1000 0000)
    {
            { ADD,  RM_IMM, 0,            PFX_WIDE, 3 },
            { OR,   RM_IMM, 0,            PFX_WIDE, 3 },
            { ADC,  RM_IMM, 0,            PFX_WIDE, 3 },
            { SBB,  RM_IMM, 0,            PFX_WIDE, 3 },
            { AND,  RM_IMM, 0,            PFX_WIDE, 3 },
            { SUB,  RM_IMM, 0,            PFX_WIDE, 3 },
            { XOR,  RM_IMM, 0,            PFX_WIDE, 3 },
            { CMP,  RM_IMM, 0,            PFX_WIDE, 3 },
    },
    // [0x01]: 0x81 (0b1000 0001)
    {
            { ADD,  RM_IMM, MASK_W,          PFX_WIDE, 4 },
            { OR,   RM_IMM, MASK_W,          PFX_WIDE, 4 },
            { ADC,  RM_IMM, MASK_W,          PFX_WIDE, 4 },
            { SBB,  RM_IMM, MASK_W,          PFX_WIDE, 4 },
            { AND,  RM_IMM, MASK_W,          PFX_WIDE, 4 },
            { SUB,  RM_IMM, MASK_W,          PFX_WIDE, 4 },
            { XOR,  RM_IMM, MASK_W,          PFX_WIDE, 4 },
            { CMP,  RM_IMM, MASK_W,          PFX_WIDE, 4 },
    },
    // [0x02]: 0x82 (0b1000 0010)
    {
            { ADD,  RM_IMM, MASK_S,          PFX_WIDE, 3 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { ADC,  RM_IMM, MASK_S,          PFX_WIDE, 3 },
            { SBB,  RM_IMM, MASK_S,          PFX_WIDE, 3 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { SUB,  RM_IMM, MASK_S,          PFX_WIDE, 3 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { CMP,  RM_IMM, MASK_S,          PFX_WIDE, 3 },
    },
    // [0x03]: 0x83 (0b1000 0011)
    {
            { ADD,  RM_IMM, MASK_S|MASK_W,      PFX_WIDE, 3 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { ADC,  RM_IMM, MASK_S|MASK_W,      PFX_WIDE, 3 },
            { SBB,  RM_IMM, MASK_S|MASK_W,      PFX_WIDE, 3 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { SUB,  RM_IMM, MASK_S|MASK_W,      PFX_WIDE, 3 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { CMP,  RM_IMM, MASK_S|MASK_W,      PFX_WIDE, 3 },
    },
    // [0x04]: 0x8C (0b1000 1100)
    {
            { MOV,  RM_SR,  MASK_ES|MASK_W,     0,        2 },
            { MOV,  RM_SR,  MASK_CS|MASK_W,     0,        2 },
            { MOV,  RM_SR,  MASK_SS|MASK_W,     0,        2 },
            { MOV,  RM_SR,  MASK_DS|MASK_W,     0,        2 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
    },
    // [0x05]: 0x8E (0b1000 1110)
    {
            { MOV,  RM_SR,  MASK_ES|MASK_D|MASK_W, 0,        2 },
            { MOV,  RM_SR,  MASK_CS|MASK_D|MASK_W, 0,        2 },
            { MOV,  RM_SR,  MASK_SS|MASK_D|MASK_W, 0,        2 },
            { MOV,  RM_SR,  MASK_DS|MASK_D|MASK_W, 0,        2 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
    },
    // [0x06]: 0x8F (0b1000 1111)
    {
            { POP,  RM,     MASK_W,          PFX_WIDE, 2 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
    },
    // [0x07]: 0xC6 (0b1100 0110)
    {
            { MOV,  RM_IMM, MASK_MO,         PFX_WIDE, 3 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
    },
    // [0x08]: 0xC7 (0b1100 0111)
    {
            { MOV,  RM_IMM, MASK_W|MASK_MO,     PFX_WIDE, 4 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
    },
    // [0x09]: 0xD0 (0b1101 0000)
    {
            { ROL,  RM_V,   0,            PFX_WIDE, 2 },
            { ROR,  RM_V,   0,            PFX_WIDE, 2 },
            { RCL,  RM_V,   0,            PFX_WIDE, 2 },
            { RCR,  RM_V,   0,            PFX_WIDE, 2 },
            { SHL,  RM_V,   0,            PFX_WIDE, 2 },
            { SHR,  RM_V,   0,            PFX_WIDE, 2 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { SAR,  RM_V,   0,            PFX_WIDE, 2 },
    },
    // [0x0A]: 0xD1 (0b1101 0001)
    {
            { ROL,  RM_V,   MASK_W,          PFX_WIDE, 2 },
            { ROR,  RM_V,   MASK_W,          PFX_WIDE, 2 },
            { RCL,  RM_V,   MASK_W,          PFX_WIDE, 2 },
            { RCR,  RM_V,   MASK_W,          PFX_WIDE, 2 },
            { SHL,  RM_V,   MASK_W,          PFX_WIDE, 2 },
            { SHR,  RM_V,   MASK_W,          PFX_WIDE, 2 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { SAR,  RM_V,   MASK_W,          PFX_WIDE, 2 },
    },
    // [0x0B]: 0xD2 (0b1101 0010)
    {
            { ROL,  RM_V,   MASK_V,          PFX_WIDE, 2 },
            { ROR,  RM_V,   MASK_V,          PFX_WIDE, 2 },
            { RCL,  RM_V,   MASK_V,          PFX_WIDE, 2 },
            { RCR,  RM_V,   MASK_V,          PFX_WIDE, 2 },
            { SHL,  RM_V,   MASK_V,          PFX_WIDE, 2 },
            { SHR,  RM_V,   MASK_V,          PFX_WIDE, 2 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { SAR,  RM_V,   MASK_V,          PFX_WIDE, 2 },
    },
    // [0x0C]: 0xD3 (0b1101 0011)
    {
            { ROL,  RM_V,   MASK_V|MASK_W,      PFX_WIDE, 2 },
            { ROR,  RM_V,   MASK_V|MASK_W,      PFX_WIDE, 2 },
            { RCL,  RM_V,   MASK_V|MASK_W,      PFX_WIDE, 2 },
            { RCR,  RM_V,   MASK_V|MASK_W,      PFX_WIDE, 2 },
            { SHL,  RM_V,   MASK_V|MASK_W,      PFX_WIDE, 2 },
            { SHR,  RM_V,   MASK_V|MASK_W,      PFX_WIDE, 2 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { SAR,  RM_V,   MASK_V| MASK_W,     PFX_WIDE, 2 },
    },
    // [0x0D]: 0xF6 (0b1111 0110)
    {
            { TEST, RM_IMM, 0,            PFX_WIDE, 3 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { NOT,  RM,     0,            PFX_WIDE, 2 },
            { NEG,  RM,     0,            PFX_WIDE, 2 },
            { MUL,  RM,     0,            PFX_WIDE, 2 },
            { IMUL, RM,     0,            PFX_WIDE, 2 },
            { DIV,  RM,     0,            PFX_WIDE, 2 },
            { IDIV, RM,     0,            PFX_WIDE, 2 },
    },
    // [0x0E]: 0xF7 (0b1111 0111)
    {
            { TEST, RM_IMM, MASK_W,          PFX_WIDE, 4 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { NOT,  RM,     MASK_W,          PFX_WIDE, 2 },
            { NEG,  RM,     MASK_W,          PFX_WIDE, 2 },
            { MUL,  RM,     MASK_W,          PFX_WIDE, 2 },
            { IMUL, RM,     MASK_W,          PFX_WIDE, 2 },
            { DIV,  RM,     MASK_W,          PFX_WIDE, 2 },
            { IDIV, RM,     MASK_W,          PFX_WIDE, 2 },
    },
    // [0x0F]: 0xFE (0b1111 1110)
    {
            { INC,  RM,     0,            PFX_WIDE, 2 },
            { DEC,  RM,     0,            PFX_WIDE, 2 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
            { UNKNOWN,  NONE,   0,            0,        1 },
    },
    // [0x10]: 0xFF (0b1111 1111)
    {
            { INC,  RM,     MASK_W|MASK_MO,     PFX_WIDE, 2 },
            { DEC,  RM,     MASK_W|MASK_MO,     PFX_WIDE, 2 },
            { CALL, RM,     MASK_W,          0,        2 },
            { CALL, RM,     MASK_W|MASK_MO,     PFX_FAR,  2 },
            { JMP,  RM,     MASK_W,          0,        2 },
            { JMP,  RM,     MASK_W|MASK_MO,     PFX_FAR,  2 },
            { PUSH, RM,     MASK_W|MASK_MO,     PFX_WIDE, 2 },
            { UNKNOWN,  NONE,   0,            PFX_WIDE, 1 },
    },
};

const char *get_instruction_name(TYPE type) {
    switch(type) {
            case AAA:    return "aaa";
            case AAD:    return "aad";
            case AAM:    return "aam";
            case AAS:    return "aas";
            case ADC:    return "adc";
            case ADD:    return "add";
            case AND:    return "and";
            case CALL:   return "call";
            case CALLF:  return "callf";
            case CBW:    return "cbw";
            case CLC:    return "clc";
            case CLD:    return "cld";
            case CLI:    return "cli";
            case CMC:    return "cmc";
            case CMP:    return "cmp";
            case CMPSB:  return "cmpsb";
            case CMPSW:  return "cmpsw";
            case CWD:    return "cwd";
            case DAA:    return "daa";
            case DAS:    return "das";
            case DEC:    return "dec";
            case DIV:    return "div";
            case ESC:    return "esc";
            case HLT:    return "hlt";
            case IDIV:   return "idiv";
            case IMUL:   return "imul";
            case IN:     return "in";
            case INC:    return "inc";
            case INT:    return "int";
            case INT3:   return "int3";
            case INTO:   return "into";
            case IRET:   return "iret";
            case JA:     return "ja";
            case JAE:    return "jae";
            case JB:     return "jb";
            case JBE:    return "jbe";
            case JCXZ:   return "jcxz";
            case JE:     return "je";
            case JG:     return "jg";
            case JGE:    return "jge";
            case JL:     return "jl";
            case JLE:    return "jle";
            case JMP:    return "jmp";
            case JMPF:   return "jmpf";
            case JNE:    return "jne";
            case JNO:    return "jno";
            case JNS:    return "jns";
            case JO:     return "jo";
            case JP:     return "jp";
            case JPO:    return "jpo";
            case JS:     return "js";
            case LAHF:   return "lahf";
            case LDS:    return "lds";
            case LEA:    return "lea";
            case LES:    return "les";
            case LOCK:   return "lock";
            case LODSB:  return "lodsb";
            case LODSW:  return "lodsw";
            case LOOP:   return "loop";
            case LOOPZ:  return "loopz";
            case LOOPNZ: return "loopnz";
            case MOV:    return "mov";
            case MOVSB:  return "movsb";
            case MOVSW:  return "movsw";
            case MUL:    return "mul";
            case NEG:    return "neg";
            case NOP:    return "nop";
            case NOT:    return "not";
            case OR:     return "or";
            case OUT:    return "out";
            case POP:    return "pop";
            case POPF:   return "popf";
            case PUSH:   return "push";
            case PUSHF:  return "pushf";
            case RCL:    return "rcl";
            case RCR:    return "rcr";
            case REP:    return "rep";
            case REPNE:  return "repne";
            case RET:    return "ret";
            case RETF:   return "retf";
            case ROL:    return "rol";
            case ROR:    return "ror";
            case SAHF:   return "sahf";
            case SAR:    return "sar";
            case SBB:    return "sbb";
            case SCASB:  return "scasb";
            case SCASW:  return "scasw";
            case SGMNT:  return "";
            case SHL:    return "shl";
            case SHR:    return "shr";
            case STC:    return "stc";
            case STD:    return "std";
            case STOSB:  return "stosb";
            case STOSW:  return "stosw";
            case STI:    return "sti";
            case SUB:    return "sub";
            case TEST:   return "test";
            case WAIT:   return "wait";
            case XCHG:   return "xchg";
            case XLAT:   return "xlat";
            case XOR:    return "xor";

            case UNKNOWN:
                    return "<invalid>";
            case EXTD:
                    assert(0 && "EXTD encountered");
    }

    return "<unknown>";
};


int parse_instruction(Instruction *instruction,  uint8 * const data, uint size, uint offset) {
    InstructionData instruction_data;
    uint8 *raw = data + offset;
    uint8 lo = 0, hi = 0, extd_op = 0;
    uint8 i = 0;
    uint8 mod, rm;
    uint data_size = 1, disp_size = 0;

    memset(instruction, 0, sizeof(*instruction));

    instruction_data = instruction_table[raw[0]];

    //fprintf(stdout, "%d\n", raw[0]);
    if (instruction_data.type == EXTD) {
        switch (raw[0]) {
        case 0x80: i = 0x00; break;
        case 0x81: i = 0x01; break;
        case 0x82: i = 0x02; break;
        case 0x83: i = 0x03; break;
        case 0x8C: i = 0x04; break;
        case 0x8E: i = 0x05; break;
        case 0x8F: i = 0x06; break;
        case 0xC6: i = 0x07; break;
        case 0xC7: i = 0x08; break;
        case 0xD0: i = 0x09; break;
        case 0xD1: i = 0x0A; break;
        case 0xD2: i = 0x0B; break;
        case 0xD3: i = 0x0C; break;
        case 0xF6: i = 0x0D; break;
        case 0xF7: i = 0x0E; break;
        case 0xFE: i = 0x0F; break;
        case 0xFF: i = 0x10; break;
        }

        extd_op = EXTD(raw[1]);
        instruction_data = instruction_table_extd[i][extd_op];
    }

    if (offset + instruction_data.size > size) {
        fprintf(stderr, "Out of bounds parsing instruction");
        return -1;
    }
    // TODO HERE..
    // set fields for [mod ... r/m]
    switch (instruction_data.format) {
        case RM_REG:
            mod = MOD(raw[1]);
            rm  = RM(raw[1]);

            // direct address and 16-bit displacement
            if (mod == MODE_MEM16 || (mod == MODE_MEM0 && rm == 0b110))
                disp_size = 2;

            // 8-bit displacement
            if (mod == MODE_MEM8)
                disp_size = 1;

            instruction_data.size += disp_size;
            instruction->displacement = (raw[3] << 8) * (disp_size > 1) | raw[2];
            instruction->fields |= (mod & MASK_MOD) << 0;
            instruction->fields |= (rm  & MASK_RM)  << 4;
            break;
        default:
            break;
    };

    // set fields for reg
    switch (instruction_data.format) {
        case REG_IMM:
            instruction->fields |= (REG2(raw[0]) & MASK_REG) << 7;
            break;
        case RM_REG:
            instruction->fields |= (REG(raw[1]) & MASK_REG) << 7;
            break;
        default:
            break;
    };


    // for data/addr fields
    switch (instruction_data.format) {
        case REG_IMM:
            if (instruction_data.flags & MASK_S) {
                instruction->data = raw[instruction_data.size - data_size];
                if (instruction->data & 0x80)
                    instruction->data |= 0xFF00;
                break;
            }

            if (W(instruction_data.flags)) {
                data_size = 2;
                hi = raw[instruction_data.size - data_size + 1];
            }

            lo = raw[instruction_data.size - data_size];
            instruction->data = (hi << 8) | lo;
            break;
        default:
            break;
    }

    if (offset + instruction_data.size > size) {
        fprintf(stderr, "out of image boundaries (offset: %u, "
                "inst_size: %u, image_size: %u)\n", offset, instruction_data.size,
                size);
        return -2;
    }

    instruction->offset = offset;
    instruction->structure = instruction_data;
    return 0;
};

int scan_instructions(Instruction *const instructions, uint count, uint8 *const data, uint size) {
    uint i;
    uint offset = 0;
    Instruction instruction;

    // we want to scan and return a count
    // so we can allocate memory for all the instructions
    if (!instructions) {
        for (count = 0; offset < size; ++count) {
            if (parse_instruction(&instruction, data, size, offset) < 0) {
                return -1;
            }
            
            if (instruction.structure.type == UNKNOWN) {
	        fprintf(stderr, "unknown instruction 0x%02X at interation %d\n", data[offset], count);
		return -2;
	    }

            offset += instruction.structure.size;
        }
        return count;
    }

    for (i = 0; i < count && offset < size; ++i) {
        if (parse_instruction(instructions + i, data, size, offset) < 0) {
            return -1;
        }
        offset += instructions[i].structure.size;
    }
    return 0;
}; 

typedef void (*decode_fn)(FILE *, Instruction *);

static void decode_rm   (FILE *out, Instruction *inst);
static void decode_reg  (FILE *out, Instruction *inst);
static void decode_imm  (FILE *out, Instruction *inst);
/*
static void decode_sr   (FILE *out, Instruction *inst);
static void decode_v    (FILE *out, Instruction *inst);
static void decode_acc  (FILE *out, Instruction *inst);
static void decode_dx   (FILE *out, Instruction *inst);
static void decode_imm8 (FILE *out, Instruction *inst);
static void decode_mem  (FILE *out, Instruction *inst);
static void decode_addr (FILE *out, Instruction *inst);
static void decode_naddr(FILE *out, Instruction *inst);
static void decode_faddr(FILE *out, Instruction *inst);
*/

void decode_rm(FILE *out, Instruction *instruction)
{
    int   len;
    char  ea_str[64];
    char *op;

    uint8  w, mod, r_m;

    int16 disp = *((int16 *)&instruction->displacement);

    static char *ea_base[8] =
    {
            "bx + si",
            "bx + di",
            "bp + si",
            "bp + di",
            "si",
            "di",
            "bp",
            "bx",
    };

    w   = W(instruction->structure.flags);
    mod = FIELD_MOD(instruction->fields);
    r_m = FIELD_RM(instruction->fields);

    op = regs[w][r_m];

    if (mod != MODE_REG) {
        if (mod == MODE_MEM0 && r_m == 0b110) {
            len = snprintf(ea_str, sizeof(ea_str), "[%u]", disp & 0xFFFF);
        } else {
            // [ ea_base + d8 ]
            if (mod == MODE_MEM8) {
                // only low byte
                disp &= 0x00FF;
                // if sign bit is set then sign-extend
                if (disp & 0x80) disp |= 0xFF00;

            // [ ea_base ]
            } else if (mod == MODE_MEM0) {
                // no displacement
                disp = 0;
            }

            len = snprintf(ea_str, sizeof(ea_str), "[%s", ea_base[r_m]);
            if (disp != 0) {
                len += snprintf(ea_str + len,
                                sizeof(ea_str) - len, " %c %d",
                                (disp < 0) ? '-' : '+',
                                abs(disp));
            }

            len += snprintf(ea_str + len, sizeof(ea_str) - len, "]");
        }

        op = ea_str;
        /*
        if (instruction->structure.prefixes & PFX_WIDE) {
                fprintf(out, "%s ", w ? "word" : "byte");
        }

        if (instruction->structure.prefixes & PFX_SGMNT) {
                fprintf(out, "%s:", segregs[SGMNT_OP(instruction->structure.prefixes)]);
        }*/
    }

    fprintf(out, "%s", op);
}

void decode_reg(FILE *out, Instruction *instruction)
{
    uint8 w, reg;

    w   = W(instruction->structure.flags);
    reg = FIELD_REG(instruction->fields);

    fprintf(out, "%s", regs[w][reg]);
}

void decode_imm(FILE *out, Instruction *instruction)
{
    int16 imm = *((int16 *)&instruction->data);
    fprintf(out, "%d", imm);
}

int decode_instruction(FILE *out, Instruction *instruction)
{
    decode_fn op1 = NULL, op2 = NULL, tmp;

    if (!out || !instruction) {
        fprintf(stderr, "invalid arguments (out: %p, image: %p)\n", out, instruction);
        return -1;
    }

    //if (instruction->structure.flags & MASK_LB) {
    //    fprintf(out, "label_%u:\n", instruction->offset);
    //}

    fprintf(out, "%s", get_instruction_name(instruction->structure.type));

    //if (instruction->structure.prefixes & PFX_FAR) fprintf(out, " far");

    switch (instruction->structure.format) {
        case RM_REG:
            op1 = decode_rm;
            op2 = decode_reg;
            break;
        case REG_IMM:
            op1 = decode_reg;
            op2 = decode_imm;
            break;
        default:
            break;
    }

    if (instruction->structure.flags & MASK_D) {
        tmp = op1;
        op1 = op2;
        op2 = tmp;
    }

    if (op1) {
        fputc(' ', out);
        op1(out, instruction);
    }

    if (op2) {
        fprintf(out, ", ");
        op2(out, instruction);
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Missing file to decode. Usage: decode <filename>\n");
        return 0;
    }

    uint size = 0;
    FILE *f = fopen(argv[1], "rb");
    assert(f != NULL);
    assert(fseek(f, 0, SEEK_END) == 0);
    size = ftell(f);
    rewind(f);

    uint8 *raw_data = malloc(size);
    if (raw_data == NULL) exit(137);
    assert(fread(raw_data, size, 1, f) == 1);

    fclose(f);

    int instruction_count = 0;
    Instruction *instructions;

    instruction_count = scan_instructions(NULL, 0, raw_data, size);
    if (instruction_count < 0) return 0;

    instructions = malloc(instruction_count * sizeof(Instruction));
    if (instructions == NULL) exit(137);

    scan_instructions(instructions, instruction_count, raw_data, size);

    int i;
    uint offset;

    fprintf(stdout, "bits 16\n\n");
    for (i=0, offset=0; i < instruction_count && offset < size; ++i, offset += instructions[i].structure.size) {
        decode_instruction(stdout, instructions + i);
        fputc('\n', stdout);
    }

    free(raw_data);
    free(instructions);
    return 0;
}