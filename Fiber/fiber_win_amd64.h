#include <Windows.h>
#include <cassert>
#define TINY_FIBER_MALLOC   malloc
#define TINY_FIBER_FREE     free

namespace Fiber {
	struct FiberContexInternal {
		LPVOID  raw_fiber_handle = nullptr;
	};

	struct Fiber {
		FiberContexInternal context;
		bool is_fiber_from_thread = false;
	};

	typedef Fiber* FiberHandle;

	inline FiberHandle CreateFiber(UINT32 stack_size, void (*fiber_func)(void*)) {
		if (stack_size == 0 || !fiber_func)
			::abort();

		Fiber* ptr = (Fiber*)TINY_FIBER_MALLOC(sizeof(Fiber));
		ptr->context.raw_fiber_handle = ::CreateFiber(stack_size, fiber_func, nullptr);
		ptr->is_fiber_from_thread = false;
		return ptr;
	}

	inline FiberHandle CreateFiberFromThread() {
		Fiber* ptr = (Fiber*)TINY_FIBER_MALLOC(sizeof(Fiber));
		ptr->context.raw_fiber_handle = ::ConvertThreadToFiber(nullptr);
		ptr->is_fiber_from_thread = true;
		return ptr;
	}

	inline void SwitchToFiber(Fiber* from_fiber, const Fiber* to_fiber) {
		if (from_fiber == nullptr || to_fiber == nullptr)
			::abort();
		::SwitchToFiber(to_fiber->context.raw_fiber_handle);
	}

	inline void DeleteFiber(FiberHandle fiber_handle) {
		if (fiber_handle == nullptr || fiber_handle->context.raw_fiber_handle == nullptr)
			return;
		if (fiber_handle->is_fiber_from_thread)
			::ConvertFiberToThread();
		else ::DeleteFiber(fiber_handle->context.raw_fiber_handle);
		fiber_handle->context.raw_fiber_handle = nullptr;
		fiber_handle->is_fiber_from_thread = false;
		TINY_FIBER_FREE(fiber_handle);
	}
}
