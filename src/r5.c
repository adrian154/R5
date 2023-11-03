#include <stdint.h>

#define OP_IMM    0x13
#define OP_LUI    0x37
#define OP_AUIPC  0x17
#define OP        0x33

#define OP_IMM_FUNCT3_ADDI       0x0
#define OP_IMM_FUNCT3_SLLI       0x1
#define OP_IMM_FUNCT3_SLTI       0x2 
#define OP_IMM_FUNCT3_SLTIU      0x3
#define OP_IMM_FUNCT3_XORI       0x4
#define OP_IMM_FUNCT3_SRLI_SRAI  0x5
#define OP_IMM_FUNCT3_ORI        0x6
#define OP_IMM_FUNCT3_ANDI       0x7

#define OP_FUNCT3_ADD_SUB  0x0
#define OP_FUNCT3_SLL      0x1
#define OP_FUNCT3_SLT      0x2
#define OP_FUNCT3_SLTU     0x3
#define OP_FUNCT3_XOR      0x4
#define OP_FUNCT3_SRL_SRA  0x5
#define OP_FUNCT3_OR       0x6
#define OP_FUNCT3_AND      0x7


typedef struct {
    uint64_t regs[32];
    uint64_t pc;
} CPU;

/* Immediate operands in instructions may be stored in one of five formats and
   are always sign-extended, which we accomplish using some struct silliness */
int64_t decode_immediate_I(uint32_t insn) {
    struct { signed int x: 12; } s;
    s.x = insn >> 20;
    return s.x;
}

int64_t decode_immediate_S(uint32_t insn) {
    struct { signed int x: 12; } s;
    s.x = (insn & 0xf80) >> 7 | (insn & 0xfe000000) >> 20;
    return s.x;
}

int64_t decode_immediate_B(uint32_t insn) {
    struct { signed int x: 13; } s;
    s.x = (insn & 0x80) << 4 | (insn & 0xf00) >> 7 | (insn & 0x7e000000) >> 20 | (insn & 0x80000000) >> 19;
    return s.x;
}

int64_t decode_immediate_U(uint32_t insn) {
    struct { signed int x: 20; } s;
    s.x = insn & 0xfffff000;
    return s.x;
}

int64_t decode_immediate_J(uint32_t insn) {
    struct { signed int x: 20; } s;
    s.x = (insn & 0xff000) | (insn & 0x100000) >> 9 | (insn & 0x7fe00000) >> 20 | (insn & 0x80000000) >> 11;
    return s.x;
}

void exec(uint32_t insn, CPU *cpu) {

    /* Lowest 6 bits are always the opcode, the other fields are speculatively
       decoded here */
    int opcode = insn & 0x7f,
        rd = (insn >> 7) & 0x1f,
        funct3 = (insn >> 12) & 0x7,
        rs1 = (insn >> 15) & 0x1f,
        rs2 = (insn >> 20) & 0x1f,
        funct7 = insn >> 25;

    switch(opcode) {
        case OP_LUI:
            cpu->regs[rd] = decode_immediate_U(insn);
            break;
        case OP_AUIPC:
            cpu->regs[rd] = decode_immediate_U(insn) + cpu->pc;
            break;
        case OP_IMM:;
            int64_t imm = decode_immediate_I(insn);
            switch(funct3) {
                case OP_IMM_FUNCT3_ADDI:
                    cpu->regs[rd] = cpu->regs[rs1] + imm;
                    break;
                case OP_IMM_FUNCT3_SLLI:
                    if((imm & 0xfc0) == 0)
                        cpu->regs[rd] = cpu->regs[rs1] << (imm & 0x3f); 
                    else
                        break; // TODO: illegal instruction
                    break;
                case OP_IMM_FUNCT3_SLTI:
                    cpu->regs[rd] = (int64_t)cpu->regs[rs1] < imm; 
                    break;
                case OP_IMM_FUNCT3_SLTIU:
                    cpu->regs[rd] = cpu->regs[rs1] < (uint64_t)imm; 
                    break;
                case OP_IMM_FUNCT3_XORI:
                    cpu->regs[rd] = cpu->regs[rs1] ^ imm;
                    break;
                case OP_IMM_FUNCT3_SRLI_SRAI:;
                    int shift = imm & 0x3f,
                        shiftType = imm & 0xfc0;
                    if(shiftType == 0x10) 
                        cpu->regs[rd] = cpu->regs[rs1] >> shift | 0xffffffffffffffffU << (64 - shift); 
                    else if(shiftType == 0)
                        cpu->regs[rd] = cpu->regs[rs1] >> shift;
                    else
                        break; // TODO: illegal instruction
                    break;
                case OP_IMM_FUNCT3_ORI:
                    cpu->regs[rd] = cpu->regs[rs1] | imm;
                    break;
                case OP_IMM_FUNCT3_ANDI:
                    cpu->regs[rd] = cpu->regs[rs1] & imm;
                    break;
            }
            break;
        case OP:
            switch(funct3) {
                case OP_FUNCT3_ADD_SUB:
                    if(funct7 == 0)
                        cpu->regs[rd] = cpu->regs[rs1] + cpu->regs[rs2];
                    else if(funct7 == 0x20)
                        cpu->regs[rd] = cpu->regs[rs1] - cpu->regs[rs2];
                    else
                        break; // TODO: illegal instruction
                    break;
                case OP_FUNCT3_SLL:
                    if(funct7 == 0)
                        cpu->regs[rd] = cpu->regs[rs1] << (cpu->regs[rs2] & 0x3f);
                    else
                        break; // TODO: illegal instruction
                    break;
                case OP_FUNCT3_SLT:
                    if(funct7 == 0)
                        cpu->regs[rd] = (int64_t)cpu->regs[rs1] < (int64_t)cpu->regs[rs2];
                    else
                        break; // TODO: illegal instruction
                    break;
                case OP_FUNCT3_SLTU:
                    if(funct7 == 0)
                        cpu->regs[rd] = cpu->regs[rs1] < cpu->regs[rs2];
                    else
                        break; // TODO: illegal instruction
                    break;
                case OP_FUNCT3_XOR:
                    if(funct7 == 0)
                        cpu->regs[rd] = cpu->regs[rs1] ^ cpu->regs[rs2];
                    else
                        break; // TODO: illegal instruction
                    break;
                case OP_FUNCT3_SRL_SRA:;
                    int shift = cpu->regs[rs2] & 0x3f;
                    if(funct7 == 0)
                        cpu->regs[rd] = cpu->regs[rs1] >> shift;
                    else if(funct7 == 0x20)
                        cpu->regs[rd] = cpu->regs[rs1] >> shift | 0xffffffffffffffffU << (64 - shift);
                    else 
                        break; // TODO: illegal instruction
                    break;
                case OP_FUNCT3_OR:
                    if(funct7 == 0)
                        cpu->regs[rd] = cpu->regs[rs1] | cpu->regs[rs2];
                    else
                        break; // TODO: illegal instruction
                    break;
                case OP_FUNCT3_AND:
                    if(funct7 == 0)
                        cpu->regs[rd] = cpu->regs[rs1] & cpu->regs[rs2];
                    else
                        break; // TODO: illegal instruction
                    break;
            }
            break;
    }

}

int main(int argc, char **argv) {
    return 0;
}