#include <stdbool.h>
#include <stdint.h>

#define OP_LUI                      0x37
#define OP_AUIPC                    0x17
#define OP_JAL                      0x6f
#define OP_JALR                     0x67
#define OP_BRANCH                   0x63
#define OP_LOAD                     0x3
#define OP_STORE                    0x23
#define OP_IMM                      0x13
#define OP_IMM32                    0x1b
#define OP_OP                       0x33
#define OP_OP32                     0x3b

#define LOAD_FUNCT3_LB              0x0
#define LOAD_FUNCT3_LH              0x1
#define LOAD_FUNCT3_LW              0x2
#define LOAD_FUNCT3_LBU             0x4
#define LOAD_FUNCT3_LHU             0x5

#define BRANCH_FUNCT3_BEQ           0x0
#define BRANCH_FUNCT3_BNE           0x1
#define BRANCH_FUNCT3_BLT           0x4
#define BRANCH_FUNCT3_BGE           0x5
#define BRANCH_FUNCT3_BLTU          0x6
#define BRANCH_FUNCT3_BGEU          0x7

#define OP_IMM_FUNCT3_ADDI          0x0
#define OP_IMM_FUNCT3_SLLI          0x1
#define OP_IMM_FUNCT3_SLTI          0x2 
#define OP_IMM_FUNCT3_SLTIU         0x3
#define OP_IMM_FUNCT3_XORI          0x4
#define OP_IMM_FUNCT3_SRLI_SRAI     0x5
#define OP_IMM_FUNCT3_ORI           0x6
#define OP_IMM_FUNCT3_ANDI          0x7

#define OP_IMM32_FUNCT3_ADDIW       0x0
#define OP_IMM32_FUNCT3_SLLIW       0x1
#define OP_IMM32_FUNCT3_SRLIW_SRAIW 0x5

#define OP_FUNCT3_ADD_SUB           0x0
#define OP_FUNCT3_SLL               0x1
#define OP_FUNCT3_SLT               0x2
#define OP_FUNCT3_SLTU              0x3
#define OP_FUNCT3_XOR               0x4
#define OP_FUNCT3_SRL_SRA           0x5
#define OP_FUNCT3_OR                0x6
#define OP_FUNCT3_AND               0x7

#define OP32_FUNCT3_ADDW_SUBW       0x0
#define OP32_FUNCT3_SLLW            0x1
#define OP32_FUNCT3_SRLW_SRAW       0x5

#define PL_USER                     0x0
#define PL_SUPERVISOR               0x1
#define PL_MACHINE                  0x3

typedef struct {
    uint64_t regs[32];
    uint64_t pc;
} CPU;

static inline bool address_misaligned(uint64_t addr) {
    return addr & 0x3;
}

/* Immediate operands in instructions may be stored in one of five formats and
   are always sign-extended, which we accomplish using struct bitfields. */
static inline int64_t decode_immediate_I(uint32_t insn) {
    struct { signed int x: 12; } s;
    s.x = insn >> 20;
    return s.x;
}

static inline int64_t decode_immediate_S(uint32_t insn) {
    struct { signed int x: 12; } s;
    s.x = (insn & 0xf80) >> 7 | (insn & 0xfe000000) >> 20;
    return s.x;
}

static inline int64_t decode_immediate_B(uint32_t insn) {
    struct { signed int x: 13; } s;
    s.x = (insn & 0x80) << 4 | (insn & 0xf00) >> 7 | (insn & 0x7e000000) >> 20 | (insn & 0x80000000) >> 19;
    return s.x;
}

static inline int64_t decode_immediate_U(uint32_t insn) {
    struct { signed int x: 20; } s;
    s.x = insn & 0xfffff000;
    return s.x;
}

static inline int64_t decode_immediate_J(uint32_t insn) {
    struct { signed int x: 20; } s;
    s.x = (insn & 0xff000) | (insn & 0x100000) >> 9 | (insn & 0x7fe00000) >> 20 | (insn & 0x80000000) >> 11;
    return s.x;
}

/* Right-shifts on negative values are undefined behavior in C, so we need to
   manually implement arithmetic right shifts. (This function relies on imple-
   mentation defined behavior, which is in theory better than relying on un-
   defined behavior.) GCC will optimize both of these functions to a single
   instruction on x86. */
static inline int64_t arithmetic_shift_right_64(int64_t value, int shift) {
    return value < 0 ? ~(~value >> shift) : value >> shift;
}

static inline int32_t arithmetic_shift_right_32(int32_t value, int shift) {
    return value < 0 ? ~(~value >> shift) : value >> shift;
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

    /* After most instructions, we increase PC by 4 to move to the next instruc-
       tion. However, there are situations where this *shouldn't* be done (i.e.
       instructions that result in a branch), so we need to keep track of
       whether such a condition has occurred. */
    bool pc_updated = false;

    uint64_t target;
    int64_t imm;
    uint32_t imm32;
    int shift, shiftType;
    bool should_branch;

    switch(opcode) {
        case OP_LUI:
            cpu->regs[rd] = decode_immediate_U(insn);
            break;
        case OP_AUIPC:
            cpu->regs[rd] = decode_immediate_U(insn) + cpu->pc;
            break;
        case OP_JAL:
            target = cpu->pc + decode_immediate_J(insn);
            if(address_misaligned(target)) {
                break; // TODO: instruction misaligned
            }
            cpu->regs[rd] = cpu->pc + 4;
            cpu->pc = target;
            pc_updated = true;
            break;
        case OP_JALR: 
            target = (cpu->pc + decode_immediate_I(insn)) & ~(uint64_t)1;
            if(address_misaligned(target)) {
                break; // TODO: instruction misaligned
            }
            cpu->regs[rd] = cpu->pc + 4;
            cpu->pc = target;
            pc_updated = true;
            break;
        case OP_BRANCH:
            switch(funct3) {
                case BRANCH_FUNCT3_BEQ:
                    should_branch = cpu->regs[rs1] == cpu->regs[rs2];
                    break;
                case BRANCH_FUNCT3_BNE:
                    should_branch = cpu->regs[rs1] != cpu->regs[rs2];
                    break;
                case BRANCH_FUNCT3_BLT:
                    should_branch = (int64_t)cpu->regs[rs1] < (int64_t)cpu->regs[rs2];
                    break;
                case BRANCH_FUNCT3_BGE:
                    should_branch = (int64_t)cpu->regs[rs1] >= (int64_t)cpu->regs[rs2];
                    break;
                case BRANCH_FUNCT3_BLTU:
                    should_branch = cpu->regs[rs1] < cpu->regs[rs2];
                    break;
                case BRANCH_FUNCT3_BGEU:
                    should_branch = cpu->regs[rs1] >= cpu->regs[rs2];
                    break;
                default:
                    break; // TODO: illegal instruction
            }
            if(should_branch) {
                target = cpu->pc + decode_immediate_B(insn);
                if(address_misaligned(target)) {
                    break; // TODO: instruction misaligned
                }
                cpu->pc = target;
                pc_updated = true;
            }
            break;
        case OP_LOAD:
            target = cpu->regs[rs1] + decode_immediate_I(insn);
            switch(funct3) {
                case LOAD_FUNCT3_LB:
                    break;
                case LOAD_FUNCT3_LH:
                    break;
                case LOAD_FUNCT3_LW:
                    break;
                case LOAD_FUNCT3_LBU:
                    break;
                case LOAD_FUNCT3_LHU:
                    break;
                default:
                    break; // TODO: illegal instruction
            }
            break;
        case OP_STORE:
            break;
        case OP_IMM:
            imm = decode_immediate_I(insn);
            switch(funct3) {
                case OP_IMM_FUNCT3_ADDI:
                    cpu->regs[rd] = cpu->regs[rs1] + imm;
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
                case OP_IMM_FUNCT3_ORI:
                    cpu->regs[rd] = cpu->regs[rs1] | imm;
                    break;
                case OP_IMM_FUNCT3_ANDI:
                    cpu->regs[rd] = cpu->regs[rs1] & imm;
                    break;
                case OP_IMM_FUNCT3_SLLI:
                    if((imm & 0xfc0) == 0)
                        cpu->regs[rd] = cpu->regs[rs1] << (imm & 0x3f); 
                    else
                        break; // TODO: illegal instruction
                    break;
                case OP_IMM_FUNCT3_SRLI_SRAI:
                    shift = imm & 0x3f;
                    shiftType = imm & 0xfc0;
                    if(shiftType == 0) 
                        cpu->regs[rd] = cpu->regs[rs1] >> shift;
                    else if(shiftType == 0x10)
                        cpu->regs[rd] = arithmetic_shift_right_64(cpu->regs[rs1], shift);
                    else
                        break; // TODO: illegal instruction
                    break;
            }
            break;
        case OP_OP:
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
                case OP_FUNCT3_SRL_SRA:
                    shift = cpu->regs[rs2] & 0x3f;
                    if(funct7 == 0)
                        cpu->regs[rd] = cpu->regs[rs1] >> shift;
                    else if(funct7 == 0x20)
                        cpu->regs[rd] = arithmetic_shift_right_64(cpu->regs[rs1], shift);
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
        case OP_IMM32:
            imm32 = decode_immediate_I(insn);
            switch(funct3) {
                case OP_IMM32_FUNCT3_ADDIW:
                    cpu->regs[rd] = (int64_t)((uint32_t)cpu->regs[rs1] + imm32);
                    break;
                case OP_IMM32_FUNCT3_SLLIW:
                    if((imm32 & 0xfe0) == 0)
                        cpu->regs[rd] = (int64_t)((uint32_t)cpu->regs[rs1] << (imm32 & 0x1f)); 
                    else
                        break; // TODO: illegal instruction
                    break;
                case OP_IMM32_FUNCT3_SRLIW_SRAIW:
                    shift = imm32 & 0x1f;
                    shiftType = imm32 & 0xfe0;
                    if(shiftType == 0) 
                        cpu->regs[rd] = (int64_t)((uint32_t)cpu->regs[rs1] >> shift);
                    else if(shiftType == 0x20)
                        cpu->regs[rd] = arithmetic_shift_right_32(cpu->regs[rs1], shift);
                    else
                        break; // TODO: illegal instruction
                    break;
                default:
                    break; // TODO: illegal instruction
            }
            break;
        case OP_OP32:
            switch(funct3) {
                case OP32_FUNCT3_ADDW_SUBW:
                    if(funct7 == 0)
                        cpu->regs[rd] = (int64_t)((uint32_t)cpu->regs[rs1] + (uint32_t)cpu->regs[rs2]);
                    else if(funct7 == 0x20)
                        cpu->regs[rd] = (int64_t)((uint32_t)cpu->regs[rs1] - (uint32_t)cpu->regs[rs2]);
                    else
                        break; // TODO: illegal instruction
                    break;
                case OP32_FUNCT3_SLLW:
                    if(funct7 == 0)
                        cpu->regs[rd] = (int64_t)((uint32_t)cpu->regs[rs1] << (cpu->regs[rs2] & 0x1f));
                    else
                        break; // TODO: illegal instruction
                    break;
                case OP32_FUNCT3_SRLW_SRAW:
                    shift = cpu->regs[rs2] & 0x1f;
                    if(funct7 == 0)
                        cpu->regs[rd] = (int64_t)((uint32_t)cpu->regs[rs1] >> shift);
                    else if(funct7 == 0x20)
                        cpu->regs[rd] = arithmetic_shift_right_32(cpu->regs[rs1], shift);
                    else 
                        break; // TODO: illegal instruction
                    break;
                default:
                    break; // TODO: illegal instruction
            }
            break;
    }

    cpu->regs[0] = 0;
    if(!pc_updated) {
        cpu->pc += 4;
    }

}

int main(int argc, char **argv) {
    return 0;
}