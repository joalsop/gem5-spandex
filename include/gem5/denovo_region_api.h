#ifndef DENOVO_H__
#define DENOVO_H__

#ifdef __cplusplus
extern "C" {
#endif

#define STATIC_ASSERT(COND,MSG) \
    typedef char static_assertion_##MSG[(COND)?1:-1]

#include <stdint.h>

enum mem_policy_t {
    MEMPOLICY_DEFAULT = 0,
    MEMPOLICY_WRITETHROUGH,
    MEMPOLICY_LINE_GRANULARITY,
    MEMPOLICY_LINE_GRANULARITY_OWNED,
    MEMPOLICY_SHARED_LINE_GRANULARITY_OWNED,
    MEM_SIM_ON,
    MEM_SIM_OFF,
};

typedef int ContextID;

    // ContextID says which devices use the specialized requests.
    // This way, CPU core1 and CPU core2 can have different policies (as in producer/consumer).
const ContextID GPU_CONTEXTS = -125; // TODO: remove this
const ContextID ALL_CONTEXTS = -126;
const ContextID THIS_CONTEXT = -127;

typedef struct {
    void* start;

    // sizes of these fields add to 64 {
    uint64_t size : 44;
    mem_policy_t read_policy : 4;
    mem_policy_t write_policy : 4;
    mem_policy_t rmw_policy : 4;
    ContextID processor : 8;
    // }
} __attribute__((packed)) denovo_region_args;

STATIC_ASSERT(sizeof(denovo_region_args) == 16, unexpected_bitpacking);

static inline void denovo_region_set(denovo_region_args args) {
    // this goes to src/arch/x86/isa/decoder/two_byte_opcodes.isa
    // the args get passed from the stack.
    asm volatile (
        ".byte 0x0F, 0x04\n"
        ".word 0x56\n"
        : /* inputs */
        : /*outputs */
        : /* clobbers */ "memory"
    );
}

static inline void mem_sim_on_all() {
    denovo_region_set({NULL, 0, MEM_SIM_ON, MEMPOLICY_DEFAULT, MEMPOLICY_DEFAULT, ALL_CONTEXTS});
}
static inline void mem_sim_off_all() {
    denovo_region_set({NULL, 0, MEM_SIM_OFF, MEMPOLICY_DEFAULT, MEMPOLICY_DEFAULT, ALL_CONTEXTS});
}
static inline void mem_sim_on_this() {
    denovo_region_set({NULL, 0, MEM_SIM_ON, MEMPOLICY_DEFAULT, MEMPOLICY_DEFAULT, THIS_CONTEXT});
}
static inline void mem_sim_off_this() {
    denovo_region_set({NULL, 0, MEM_SIM_OFF, MEMPOLICY_DEFAULT, MEMPOLICY_DEFAULT, THIS_CONTEXT});
}

#ifdef __cplusplus
}
#endif

#endif
