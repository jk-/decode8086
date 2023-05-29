#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

typedef unsigned int uint;
typedef uint8_t      uint8;
typedef uint16_t     uint16;
typedef uint32_t     uint32;
typedef int8_t       int8;
typedef int16_t      int16;

#define MASK_W    0b001
#define MASK_D    0b100

#define MASK_MOD  0b011
#define MASK_RM   0b111
#define MASK_REG  0b111

#define MOD(byte)  ((byte >> 6) & MASK_MOD)
#define RM(byte)   ((byte >> 0) & MASK_RM)
#define REG(byte)  ((byte >> 3) & MASK_REG)
#define REG2(byte) ((byte >> 0) & MASK_REG)

#define FIELD_RM(fields)  ((fields >> 4) & MASK_RM)
#define FIELD_REG(fields) ((fields >> 7) & MASK_REG)

#define W(flags) (!!(flags & MASK_W))

static const char *regs[2][8] = {
    { "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh" },
    { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di" }
};

typedef enum {
    UNKNOWN,
    MOV, 
} TYPE;

typedef enum {
    RM_REG,
    REG_IMM,
} FORMAT;

typedef struct {
    TYPE   type;
    FORMAT format;
    uint8  flags;
    uint8  size;
} InstructionData;

typedef struct {
    InstructionData data;
    uint16          fields;
} Instruction;

InstructionData instruction_table[0xC0] = {
          // TYPE, FORMAT     ,  FLAGS,       SIZE 
    [0x88]= { MOV, RM_REG,  0,              2 }, // 0x88
            { MOV, RM_REG,  MASK_W,         2 }, // 0x89
            { MOV, RM_REG,  MASK_D,         2 }, // 0x8A
            { MOV, RM_REG,  MASK_D|MASK_W,  2 }, // 0x8B
    [0xB0]= { MOV, REG_IMM, 0,              2 }, // 0xB0
            { MOV, REG_IMM, 0,              2 }, // 0xB1
            { MOV, REG_IMM, 0,              2 }, // 0xB2
            { MOV, REG_IMM, 0,              2 }, // 0xB3
            { MOV, REG_IMM, 0,              2 }, // 0xB4
            { MOV, REG_IMM, 0,              2 }, // 0xB5
            { MOV, REG_IMM, 0,              2 }, // 0xB6
            { MOV, REG_IMM, 0,              2 }, // 0xB7
            { MOV, REG_IMM, MASK_W,         3 }, // 0xB8
            { MOV, REG_IMM, MASK_W,         3 }, // 0xB9
            { MOV, REG_IMM, MASK_W,         3 }, // 0xBA
            { MOV, REG_IMM, MASK_W,         3 }, // 0xBB
            { MOV, REG_IMM, MASK_W,         3 }, // 0xBC
            { MOV, REG_IMM, MASK_W,         3 }, // 0xBD
            { MOV, REG_IMM, MASK_W,         3 }, // 0xBE
            { MOV, REG_IMM, MASK_W,         3 }, // 0xBF
};

const char *get_instruction_name(TYPE type) {
    switch (type) {
        case MOV: return "mov";
        default:
            return "<invalid>";
    };
};

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Missing file to decode. Usage: decode <filename>\n");
        return 0;
    }

    size_t size = 0;
    FILE *f = fopen(argv[1], "r");
    assert(f != NULL);
    assert(fseek(f, 0, SEEK_END) == 0);
    size = ftell(f);
    assert(fseek(f, 0, SEEK_SET) == 0);

    uint8 *raw_data = (uint8 *) malloc(size);
    assert(fread(raw_data, size, 1, f) == 1);

    fclose(f);

    // todo conintue..
    // raw_data is the file bytes, not the instruction bytes
    // so if we have more than one instruction, the size of the
    // instruction data will tell us how much to offset for the next instruction

    InstructionData instruction_data = instruction_table[raw_data[0]];

    Instruction instruction = {0};

    // set fields for [mod ... r/m]
    switch (instruction_data.format) {
        case RM_REG:
            instruction.fields |= (MOD(raw_data[1]) & MASK_MOD) << 0;
            instruction.fields |= (RM(raw_data[1])  & MASK_RM)  << 4;
        default:
            break;
    };

    // set fields for reg
    switch (instruction_data.format) {
        case RM_REG:
            instruction.fields |= (REG(raw_data[1]) & MASK_REG) << 7;
        default:
            break;
    };

    instruction.data = instruction_data;

    uint8 w;
    w = W(instruction_data.flags);
    printf("bits 16\n\n");
    printf("%s ",  get_instruction_name(instruction.data.type));    
    printf("%s, ", regs[w][FIELD_RM(instruction.fields)]); 
    printf("%s\n", regs[w][FIELD_REG(instruction.fields)]); 

    free(raw_data);
    return 0;
}