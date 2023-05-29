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
    uint            offset;
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
            return "<unknown>";
    };
};


int parse_instruction(Instruction *instruction, const uint8 *data, uint size, uint offset) {
    InstructionData instruction_data;
    uint8 *raw = data + offset;
    memset(instruction, 0, sizeof(*instruction));

    instruction_data = instruction_table[raw[0]];

    if (offset + instruction_data.size > size) {
        fprintf(stderr, "Out of bounds parsing instruction");
        return -1;
    }

    // set fields for [mod ... r/m]
    switch (instruction_data.format) {
        case RM_REG:
            instruction->fields |= (MOD(raw[1]) & MASK_MOD) << 0;
            instruction->fields |= (RM(raw[1])  & MASK_RM)  << 4;
        default:
            break;
    };

    // set fields for reg
    switch (instruction_data.format) {
        case RM_REG:
            instruction->fields |= (REG(raw[1]) & MASK_REG) << 7;
        default:
            break;
    };

    instruction->offset = offset;
    instruction->data = instruction_data;
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
            offset += instruction.data.size;
        }
        return count;
    }

    for (i = 0; i < count && offset < size; ++i) {
        if (parse_instruction(instructions + i, data, size, offset) < 0) {
            return -1;
        }
        offset += instructions[i].data.size;
    }
    return 0;
}; 

void decode_instruction(FILE *out, Instruction *instruction) {
    uint8 w;
    w = W(instruction->data.flags);
    fprintf(out, "%s ",  get_instruction_name(instruction->data.type));    
    fprintf(out, "%s, ", regs[w][FIELD_RM(instruction->fields)]); 
    fprintf(out, "%s\n", regs[w][FIELD_REG(instruction->fields)]); 
};


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
    for (i=0, offset=0; i < instruction_count && offset < size; ++i, offset += instructions[i].data.size) {
        decode_instruction(stdout, instructions + i);
    }

    free(raw_data);
    free(instructions);
    return 0;
}