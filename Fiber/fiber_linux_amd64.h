#include <memory>

#define TINY_FIBER_MALLOC   malloc
#define TINY_FIBER_FREE     free

namespace Fiber {
	// an alias for CPU registers, all need to be 64 bits long
	typedef unsigned long long  Register;

	// check the register size, making sure it is 8 bytes.
	static_assert(sizeof(Register) == 8, "Incorrect register size");

    //! Fiber context that saves all the callee saved registers
/**
* The specific set of register is architecture and OS dependent. SORT uses this implementation for
* Intel Mac and X64 Ubuntu, which use 'System V AMD64 ABI'.
* https://en.wikipedia.org/wiki/X86_calling_conventions#cite_note-AMD-28
*
* System V Application Binary Interface AMD64 Architecture Processor Supplement
* Page 23, AMD64 ABI Draft 1.0
* https://github.com/hjl-tools/x86-psABI/wiki/x86-64-psABI-1.0.pdf
*/
	struct FiberContexInternal
	{
		// callee-saved registers
		Register rbx;
		Register rbp;
		Register r12;
		Register r13;
		Register r14;
		Register r15;

		// parameter registers
		Register rdi;

		// stack and instruction registers
		Register rsp;
		Register rip;
	};

#define CHECK_OFFSET(REGISTER, OFFSET) static_assert(offsetof(FiberContexInternal, REGISTER) == OFFSET, "Incorrect register offset")

// Both x64 and arm64 require stack memory pointer to be 16 bytes aligned
#define FIBER_STACK_ALIGNMENT   16

#define FIBER_REG_RBX			0x00
#define FIBER_REG_RBP			0x08
#define FIBER_REG_R12			0x10
#define FIBER_REG_R13			0x18
#define FIBER_REG_R14			0x20
#define FIBER_REG_R15			0x28
#define FIBER_REG_RDI			0x30
#define FIBER_REG_RSP			0x38
#define FIBER_REG_RIP			0x40

	CHECK_OFFSET(rbx, FIBER_REG_RBX);
	CHECK_OFFSET(rbp, FIBER_REG_RBP);
	CHECK_OFFSET(r12, FIBER_REG_R12);
	CHECK_OFFSET(r13, FIBER_REG_R13);
	CHECK_OFFSET(r14, FIBER_REG_R14);
	CHECK_OFFSET(r15, FIBER_REG_R15);
	CHECK_OFFSET(rdi, FIBER_REG_RDI);
	CHECK_OFFSET(rsp, FIBER_REG_RSP);
	CHECK_OFFSET(rip, FIBER_REG_RIP);

	asm(
	R"(
	.text
	.align 4
	_switch_fiber_internal:
	    // Save context 'from'
	    // Store callee-preserved registers
	    movq        %rbx, 0x00(%rdi) /* FIBER_REG_RBX */
	    movq        %rbp, 0x08(%rdi) /* FIBER_REG_RBP */
	    movq        %r12, 0x10(%rdi) /* FIBER_REG_R12 */
	    movq        %r13, 0x18(%rdi) /* FIBER_REG_R13 */
	    movq        %r14, 0x20(%rdi) /* FIBER_REG_R14 */
	    movq        %r15, 0x28(%rdi) /* FIBER_REG_R15 */
	    /* call stores the return address on the stack before jumping */
	    movq        (%rsp), %rcx             
	    movq        %rcx, 0x40(%rdi) /* FIBER_REG_RIP */
	
	    /* skip the pushed return address */
	    leaq        8(%rsp), %rcx            
	    movq        %rcx, 0x38(%rdi) /* FIBER_REG_RSP */
	    // Load context 'to'
	    movq        %rsi, %r8
	    // Load callee-preserved registers
	    movq        0x00(%r8), %rbx /* FIBER_REG_RBX */
	    movq        0x08(%r8), %rbp /* FIBER_REG_RBP */
	    movq        0x10(%r8), %r12 /* FIBER_REG_R12 */
	    movq        0x18(%r8), %r13 /* FIBER_REG_R13 */
	    movq        0x20(%r8), %r14 /* FIBER_REG_R14 */
	    movq        0x28(%r8), %r15 /* FIBER_REG_R15 */
	    // Load first parameter, this is only used for the first time a fiber gains control
	    movq        0x30(%r8), %rdi /* FIBER_REG_RDI */
	    // Load stack pointer
	    movq        0x38(%r8), %rsp /* FIBER_REG_RSP */
	    // Load instruction pointer, and jump
	    movq        0x40(%r8), %rcx /* FIBER_REG_RIP */
	    jmp         *%rcx
	)");

	extern "C" 
	{
	    extern void __attribute__((noinline)) _switch_fiber_internal(FiberContexInternal* from, const FiberContexInternal* to);
	}

	// create a new fiber
	inline bool _create_fiber_internal(void* stack, uint32_t stack_size, void (*target)(void*), void* arg, FiberContexInternal* context) {
		// it is the users responsibility to make sure the stack is 16 bytes aligned.
		if((((uintptr_t)stack) & (FIBER_STACK_ALIGNMENT - 1)) != 0)
			return false;

		uintptr_t* stack_top = (uintptr_t*)((uint8_t*)(stack) + stack_size);
		context->rip = (uintptr_t)target;
		context->rdi = (uintptr_t)arg;
		context->rsp = (uintptr_t)&stack_top[-3];
		stack_top[-2] = 0;

		return true;
	}

	struct Fiber
	{
		FiberContexInternal context;
		void* stack_ptr = nullptr;
		unsigned int stack_size = 0;
		bool is_fiber_from_thread = false;
	};

	typedef Fiber*  FiberHandle;

	//! Allocate stack memory for the fiber. If there is no valid function pointer provided, it will fail.
	inline FiberHandle CreateFiber(uint32_t stack_size, void (*fiber_func)(void*), void* arg = nullptr) {
		if(stack_size == 0 || !fiber_func)
			return nullptr;
		
		Fiber* ptr = (Fiber*)TINY_FIBER_MALLOC(sizeof(Fiber));

		ptr->context = {};
		ptr->stack_ptr = TINY_FIBER_MALLOC(stack_size + FIBER_STACK_ALIGNMENT - 1);
		ptr->stack_size = stack_size;

		if(!ptr->stack_ptr)
			return nullptr;
		std::cout << std::format("Stack allocated : {}\n", ptr->stack_ptr);
		// Make sure the stack meets the alignment requirement
		uintptr_t aligned_stack_ptr = (uintptr_t)ptr->stack_ptr;
		aligned_stack_ptr += FIBER_STACK_ALIGNMENT - 1;
		aligned_stack_ptr &= ~(FIBER_STACK_ALIGNMENT - 1);

		if(!_create_fiber_internal((void*)aligned_stack_ptr, stack_size, fiber_func, arg, &ptr->context))
			return nullptr;
		
		return ptr;
	}

	// Note, on Ubuntu, this doesn't really convert the current thread to a new fiber,
	// it really just creates a brand new fiber that has nothing to do with the current thread.
	// However, as long as the thread first switch to a fiber from this created fiber,
	// this would allow the thread fiber to capture the thread context.
	inline FiberHandle CreateFiberFromThread() {
		Fiber* ptr = (Fiber*)TINY_FIBER_MALLOC(sizeof(Fiber));
		ptr->context = {};
		ptr->stack_ptr = nullptr;
		ptr->stack_size = 0;
		ptr->is_fiber_from_thread = true;
		return ptr;
	}

	// The upper level logic should have checked for same fiber switch.
	// The low level implementation defined here won't do such a check.
	// In the case of switching to a same fiber, it should not crash the system. But there could be some loss of performance.
	// Another more important thing that derserve our attention is that the high level code will have
	// to make sure the from_fiber is the current executing fiber, otherwise there will be undefined behavior.
	inline void SwitchToFiber(Fiber* from_fiber, const Fiber* to_fiber) {
		_switch_fiber_internal(&from_fiber->context, &to_fiber->context);
	}

	// If the fiber is converted from a thread, delete fiber will convert the fiber back to a regular thread then.
	inline void DeleteFiber(FiberHandle fiber_handle) {
		if(fiber_handle == nullptr)
			return;
		if(fiber_handle->stack_ptr != nullptr && !fiber_handle->is_fiber_from_thread) {
			TINY_FIBER_FREE(fiber_handle->stack_ptr);
			fiber_handle->stack_ptr = nullptr;
		}
		TINY_FIBER_FREE(fiber_handle);
	}
}