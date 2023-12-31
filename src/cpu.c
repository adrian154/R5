#include <stdbool.h>
#include "cpu.h"
#include "bus.h"

// Extension defines
#define EXT_M

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
#define OP_MISC_MEM                 0xf
#define OP_SYSTEM                   0x73

#define LOAD_FUNCT3_LB              0x0
#define LOAD_FUNCT3_LH              0x1
#define LOAD_FUNCT3_LW              0x2
#define LOAD_FUNCT3_LBU             0x4
#define LOAD_FUNCT3_LHU             0x5
#define LOAD_FUNCT3_LWU             0x6
#define LOAD_FUNCT3_LD              0x3

#define STORE_FUNCT3_SB             0x0
#define STORE_FUNCT3_SH             0x1
#define STORE_FUNCT3_SW             0x2
#define STORE_FUNCT3_SD             0x3

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

#define MISC_MEM_FUNCT3_FENCE       0x0

#define SYSTEM_FUNCT3_ECALL_EBREAK  0x0

// Fence modes
#define FENCE_MODE_NORMAL           0x0
#define FENCE_MODE_TSO              0x8

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

void exec32(uint32_t insn, CPU *cpu) {

    /* Lowest 6 bits are always the opcode, the other fields are speculatively
       decoded here */
    int opcode = insn & 0x7f,
        rd = (insn >> 7) & 0x1f,
        funct3 = (insn >> 12) & 0x7,
        rs1 = (insn >> 15) & 0x1f,
        rs2 = (insn >> 20) & 0x1f,
        funct7 = insn >> 25;

    /* By default we increase PC by 4 to move to the next instruction, but we
       shouldn't do this after a branch, so keep track of whether PC was 
       updated by the instruction. */
    bool pc_updated = false;

    /* Variables used by instruction execution */
    uint64_t target;
    int64_t imm;
    uint32_t imm32;
    int shift, shift_type;
    int fenceMode;
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
                    cpu->regs[rd] = (int8_t)load8(target);
                    break;
                case LOAD_FUNCT3_LH:
                    cpu->regs[rd] = (int16_t)load16(target);
                    break;
                case LOAD_FUNCT3_LW:
                    cpu->regs[rd] = (int32_t)load32(target);
                    break;
                case LOAD_FUNCT3_LBU:
                    cpu->regs[rd] = load8(target);
                    break;
                case LOAD_FUNCT3_LHU:
                    cpu->regs[rd] = load16(target);
                    break;
                case LOAD_FUNCT3_LWU:
                    cpu->regs[rd] = load32(target);
                    break;
                case LOAD_FUNCT3_LD:
                    cpu->regs[rd] = load64(target);
                    break;
                default:
                    break; // TODO: illegal instruction
            }
            break;
        case OP_STORE:
            target = cpu->regs[rs1] + decode_immediate_S(insn);
            switch(funct3) {
                case STORE_FUNCT3_SB:
                    store8(target, cpu->regs[rs2]);
                    break;
                case STORE_FUNCT3_SH:
                    store16(target, cpu->regs[rs2]);
                    break;
                case STORE_FUNCT3_SW:
                    store32(target, cpu->regs[rs2]);
                    break;
                case STORE_FUNCT3_SD:
                    store64(target, cpu->regs[rs2]);
                    break;
                default:
                    break; // TODO: illegal instruction
            }
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
                    shift_type = imm & 0xfc0;
                    if(shift_type == 0) 
                        cpu->regs[rd] = cpu->regs[rs1] >> shift;
                    else if(shift_type == 0x10)
                        cpu->regs[rd] = (int64_t)cpu->regs[rs1] >> shift;
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
                        cpu->regs[rd] = cpu->regs[rs1] >> shift;
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
                    cpu->regs[rd] = (int32_t)((uint32_t)cpu->regs[rs1] + imm32);
                    break;
                case OP_IMM32_FUNCT3_SLLIW:
                    if((imm32 & 0xfe0) == 0)
                        cpu->regs[rd] = (int32_t)((uint32_t)cpu->regs[rs1] << (imm32 & 0x1f)); 
                    else
                        break; // TODO: illegal instruction
                    break;
                case OP_IMM32_FUNCT3_SRLIW_SRAIW:
                    shift = imm32 & 0x1f;
                    shift_type = imm32 & 0xfe0;
                    if(shift_type == 0) 
                        cpu->regs[rd] = (int32_t)((uint32_t)cpu->regs[rs1] >> shift);
                    else if(shift_type == 0x20)
                        cpu->regs[rd] = (int32_t)cpu->regs[rs1] >> shift;
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
                        cpu->regs[rd] = (int32_t)((uint32_t)cpu->regs[rs1] + (uint32_t)cpu->regs[rs2]);
                    else if(funct7 == 0x20)
                        cpu->regs[rd] = (int32_t)((uint32_t)cpu->regs[rs1] - (uint32_t)cpu->regs[rs2]);
                    else
                        break; // TODO: illegal instruction
                    break;
                case OP32_FUNCT3_SLLW:
                    if(funct7 == 0)
                        cpu->regs[rd] = (int32_t)((uint32_t)cpu->regs[rs1] << (cpu->regs[rs2] & 0x1f));
                    else
                        break; // TODO: illegal instruction
                    break;
                case OP32_FUNCT3_SRLW_SRAW:
                    shift = cpu->regs[rs2] & 0x1f;
                    if(funct7 == 0)
                        cpu->regs[rd] = (int32_t)((uint32_t)cpu->regs[rs1] >> shift);
                    else if(funct7 == 0x20)
                        cpu->regs[rd] = (int32_t)cpu->regs[rs1] >> shift;
                    else 
                        break; // TODO: illegal instruction
                    break;
                default:
                    break; // TODO: illegal instruction
            }
            break;
        case OP_MISC_MEM:
            switch(funct3) {
                case MISC_MEM_FUNCT3_FENCE:
                    /* Since our EEI only supports a single hart and all memory
                       operations are immediately executed, FENCE is a no-op. */
                    break;
                default:
                    break; // TODO: illegal instruction
            }
            break;
        case OP_SYSTEM:
            switch(funct3) {
                case SYSTEM_FUNCT3_ECALL_EBREAK:
                    
                    break;
                default:
                    break; // TODO: illegal instruction
            }
            break;
        default: 
            break; // TODO: illegal instruction
    }

    if(!pc_updated) {
        cpu->pc += 4;
    }

    /* x0 must always be zero */
    cpu->regs[0] = 0;

}

int main(int argc, char **argv) {
    return 0;
}