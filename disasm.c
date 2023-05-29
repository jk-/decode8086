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

#define MASK_W  0b01
#define MASK_D  0b10

#define MOD(byte)  ((byte >> 6) & 0b011)
#define RM(byte)   ((byte >> 0) & 0b111)
#define REG(byte)  ((byte >> 3) & 0b111)
#define REG2(byte) ((byte >> 0) & 0b111)

const char *regs[2][8] = {
    { "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh" },
    { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di" }
};

enum inst_type {
    UNK = 0,
    MOV, MOVSB, MOVSW,
    EXTD,
};

enum inst_format {
    FMT_RM_REG,
    FMT_REG_IMM,
};

struct inst_data {
    enum inst_type   type;
    enum inst_format fmt;
         uint8       flags;
         uint8       prefixes;
         uint8       size;
};

struct inst {
    struct inst_data    base;
           uint16       disp;
           uint16       data;     // addr, imm
           uint16       data_ext; // if instruction size is 6 bytes
           uint16       fields;   // mod, reg, r/m, sr, esc
           uint         offset;   // location in image
};

struct inst_data inst_table[256] = {
    [0x88]= { MOV, FMT_RM_REG,  0,       0, 2 }, // 0x88
            { MOV, FMT_RM_REG,  MASK_W,     0, 2 }, // 0x89
            { MOV, FMT_RM_REG,  MASK_D,     0, 2 }, // 0x8A
            { MOV, FMT_RM_REG,  MASK_D|MASK_W, 0, 2 }, // 0x8B
    [0xB0]= { MOV, FMT_REG_IMM, 0,       0, 2 }, // 0xB0
            { MOV, FMT_REG_IMM, 0,       0, 2 }, // 0xB1
            { MOV, FMT_REG_IMM, 0,       0, 2 }, // 0xB2
            { MOV, FMT_REG_IMM, 0,       0, 2 }, // 0xB3
            { MOV, FMT_REG_IMM, 0,       0, 2 }, // 0xB4
            { MOV, FMT_REG_IMM, 0,       0, 2 }, // 0xB5
            { MOV, FMT_REG_IMM, 0,       0, 2 }, // 0xB6
            { MOV, FMT_REG_IMM, 0,       0, 2 }, // 0xB7
            { MOV, FMT_REG_IMM, MASK_W,     0, 3 }, // 0xB8
            { MOV, FMT_REG_IMM, MASK_W,     0, 3 }, // 0xB9
            { MOV, FMT_REG_IMM, MASK_W,     0, 3 }, // 0xBA
            { MOV, FMT_REG_IMM, MASK_W,     0, 3 }, // 0xBB
            { MOV, FMT_REG_IMM, MASK_W,     0, 3 }, // 0xBC
            { MOV, FMT_REG_IMM, MASK_W,     0, 3 }, // 0xBD
            { MOV, FMT_REG_IMM, MASK_W,     0, 3 }, // 0xBE
            { MOV, FMT_REG_IMM, MASK_W,     0, 3 }, // 0xBF
};

const char *get_instruction_name(enum inst_type type) {
    switch (type) {
        case MOV: return "mov";
        default:
            return "<invalid>";
    };
};

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Missing file to disasm\n");
        return 0;
    }
    
    size_t size = 0;
    FILE *f = fopen(argv[1], "r");
    assert(f != NULL);
    assert(fseek(f, 0, SEEK_END) == 0);
    size = ftell(f);
    assert(fseek(f, 0, SEEK_SET) == 0);

    uint8_t *data = malloc(size);
    if (data == NULL) exit(127);
    assert(fread(data, size, 1, f) == 1);

    fclose(f);

    struct inst_data mov = inst_table[data[0]];

    printf("bits 16\n\n");
    printf("%s", get_instruction_name(mov.type));    
    switch (mov.type) {
        case MOV:
            if (mov.fmt == FMT_RM_REG) {
                uint8 mod, rm, reg;
                mod = MOD(data[1]);
                rm  = RM(data[1]); 
                reg = REG(data[1]);

            }
            break;
        default:
            printf("instruction not found\n");
    };

    free(data);
    return 0;

}