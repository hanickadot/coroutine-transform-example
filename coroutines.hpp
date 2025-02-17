#ifndef COROUTINES_HPP
#define COROUTINES_HPP

#include <memory>
#include <concepts>

namespace hana {

// coroutine_base type and pointer to next piece of coroutines
struct coroutine_base;
using coroutine_jump_t = void (*)(coroutine_base *);

// base is just pointer to next coroutine (commont to ALL coroutines)
struct coroutine_base {
	coroutine_jump_t __next = nullptr;

	constexpr coroutine_base() noexcept { }
	constexpr coroutine_base(coroutine_jump_t next) noexcept: __next{next} { }
	virtual constexpr ~coroutine_base() noexcept = default;
};

// helper function to resume/start coroutine
constexpr void jumpto(coroutine_base * base) {
	[[clang::musttail]] return base->__next(base);
}

// all coroutines with same promise_type shares same coroutine_promise_base<Promise> type
template <typename Promise> struct coroutine_promise_base: coroutine_base {
	constexpr coroutine_promise_base(): coroutine_base{} { }

	Promise __promise{};
};

// coroutine handle is a (non-owning) pointer to coroutine_base
template <typename Promise> struct coroutine_handle;

// generic
template <> struct coroutine_handle<void> {
	coroutine_base * coro;

	constexpr bool done() const noexcept {
		return coro->__next == nullptr;
	}

	constexpr void resume() const {
		return hana::jumpto(coro);
	}

	constexpr void destroy() const {
		delete coro;
	}

	constexpr coroutine_handle & operator=(nullptr_t) noexcept {
		coro = nullptr;
		return *this;
	}

	constexpr friend bool operator==(coroutine_handle lhs, nullptr_t) {
		return lhs.coro == nullptr;
	}
};

// or typed
template <typename Promise> struct coroutine_handle {
	coroutine_promise_base<Promise> * coro;

	constexpr operator coroutine_handle<void>() const noexcept {
		return coroutine_handle<void>{coro};
	}

	constexpr bool done() const noexcept {
		return coro->__next == nullptr;
	}

	constexpr void resume() const {
		return hana::jumpto(coro);
	}

	constexpr void destroy() const {
		delete coro;
	}

	constexpr Promise & promise() const {
		return coro->__promise;
	}

	constexpr coroutine_handle & operator=(nullptr_t) noexcept {
		coro = nullptr;
		return *this;
	}

	constexpr friend bool operator==(coroutine_handle lhs, nullptr_t) {
		return lhs.coro == nullptr;
	}
};

template <typename T> coroutine_handle(T *) -> coroutine_handle<decltype(T::__promise)>;

// awaiters
struct suspend_always {
	constexpr bool await_ready() const noexcept {
		return false;
	}
	constexpr void await_suspend(hana::coroutine_handle<void>) noexcept {
		// does nothing
	}
	constexpr void await_resume() {
	}
};

struct suspend_never {
	constexpr bool await_ready() const noexcept {
		return true;
	}
	constexpr void await_suspend(hana::coroutine_handle<void>) noexcept {
		// does nothing
	}
	constexpr void await_resume() {
	}
};

// some concepts
template <typename T> concept coroutine_state = std::convertible_to<T, coroutine_promise_base<decltype(T::__promise)>>;

template <typename T> concept suspend_will_always_suspend = std::same_as<T, void>;
template <typename T> concept suspend_will_sometimes_suspend = std::same_as<T, bool>;
template <typename T> concept suspend_will_jump_somewhere = std::convertible_to<T, coroutine_handle<void>>;

template <typename T> concept awaitable_suspend_return = suspend_will_always_suspend<T> || suspend_will_sometimes_suspend<T> || suspend_will_jump_somewhere<T>;

template <typename T, typename Promise> concept awaitable = requires(T & obj, const T & cobj, coroutine_handle<Promise> hndl) {
	{ cobj.await_ready() } -> std::same_as<bool>;
	{ obj.await_suspend(hndl) } -> awaitable_suspend_return;
	{ obj.await_resume() };
};

// noop coroutine is usable when we want to stop jumping around
struct noop_coroutine_state: coroutine_base {
	static constexpr void return_to_caller(coroutine_base *) noexcept { }
	constexpr noop_coroutine_state(coroutine_jump_t resume) noexcept: coroutine_base{resume} { }
	virtual ~noop_coroutine_state() noexcept = default;
};

static constexpr auto noop_coroutine_val = noop_coroutine_state{noop_coroutine_state::return_to_caller};

constexpr coroutine_base * noop_coroutine() noexcept {
	return const_cast<noop_coroutine_state *>(&noop_coroutine_val);
}

// select where to go next, to continuation or somewhere else or to our caller?
template <typename T, typename Promise = decltype(T::__promise)> constexpr coroutine_base * select_next_or(awaitable<Promise> auto && obj, T * current, coroutine_jump_t continuation) {
	const auto handle = coroutine_handle{current};
	using R = decltype(obj.await_suspend(handle));

	current->__next = continuation;

	// TODO exceptions

	// no need to suspend, just jump there
	if (!obj.await_ready()) {
		if constexpr (suspend_will_always_suspend<R>) {
			obj.await_suspend(handle);
			return noop_coroutine(); // will return to caller

		} else if constexpr (suspend_will_sometimes_suspend<R>) {
			if (obj.await_suspend(handle)) {
				return noop_coroutine(); // will return to caller
			}

		} else {
			static_assert(suspend_will_jump_somewhere<R>);
			return obj.await_suspend(handle).__coro;
		}
	}

	return current; // will resume continuation
}

// access to coroutine local variable
#define CORO_VAR(NAME) (static_cast<self *>(__vstate)->NAME)

// await on awaiter, and resume on continuation
#define CORO_AWAIT(AWAITER, CONTINUATION) [[clang::musttail]] return hana::jumpto(hana::select_next_or(AWAITER, static_cast<self *>(__vstate), CONTINUATION))

// start current coroutine
#define CORO_START() __coro_initial_suspend<self>(__vstate)

#define CORO_JUMP_TO_OTHER(COROUTINE) [[clang::musttail]] return jumpto(COROUTINE)

// unconditionaly continue somewhere else...
#define CORO_JUMP(CONTINUATION) [[clang::musttail]] return CONTINUATION(__vstate);

// obtain handle to current coroutine
#define CORO_HANDLE_FROM_STATE() \
	hana::coroutine_handle { static_cast<self *>(__vstate) }

#define CORO_FINAL_SUSPEND() [[clang::musttail]] return hana::__coro_final_suspend<self>(__vstate)

// for final suspend
#define CORO_DESTROY_ITSELF() delete static_cast<self *>(__vstate)

// helper functions... normal coroutine transform...
template <typename self, typename... Args> static constexpr auto wrapper(Args &&... args) {
	self * __vstate = new self;

	// copy arguments (ignored here)
	// construct promise

	// divergence because std::coroutine_handle<Promise>::from_promise(*this) can't be done in constexpr (without magic), normally this doesn't take any argument
	auto result = CORO_VAR(__promise).get_return_object(CORO_HANDLE_FROM_STATE());

	CORO_START();

	return result;
}

// initial suspend support
template <typename self> static constexpr void __coro_resume_initial_suspend(hana::coroutine_base * __vstate) {
	CORO_VAR(initial_awaiter).await_resume();
	std::destroy_at(&CORO_VAR(initial_awaiter));

	CORO_JUMP(self::__coro_body);
}

template <typename self> static constexpr void __coro_initial_suspend(hana::coroutine_base * __vstate) {
	std::construct_at(&CORO_VAR(initial_awaiter), CORO_VAR(__promise).initial_suspend());

	CORO_AWAIT(CORO_VAR(initial_awaiter), (__coro_resume_initial_suspend<self>));
}

// final suspend
template <typename self> static constexpr void __coro_final_suspend(hana::coroutine_base * __vstate) {
	{ // so we can do safely tail call, awaiter must be destroyed
		auto awaiter = CORO_VAR(__promise).final_suspend();

		__vstate->__next = nullptr; // so .done() check will work

		// we won't suspend => destroy coroutine state
		if (awaiter.await_ready()) {
			CORO_DESTROY_ITSELF();
			return;
		}

		using R = decltype(awaiter.await_suspend(CORO_HANDLE_FROM_STATE()));

		if constexpr (suspend_will_always_suspend<R>) {
			awaiter.await_suspend(CORO_HANDLE_FROM_STATE());
			return; // return to our caller

		} else if constexpr (suspend_will_sometimes_suspend<R>) {
			if (!awaiter.await_suspend(CORO_HANDLE_FROM_STATE())) {
				CORO_DESTROY_ITSELF(); // destroy if we didn't suspend
			}
			return; // and return to our caller

		} else {
			static_assert(suspend_will_jump_somewhere<R>);
			// jump somewhere else (reusing variable)
			__vstate = awaiter.await_suspend(CORO_HANDLE_FROM_STATE()).__coro;
			// and fallthrough
		}
	}

	CORO_JUMP_TO_OTHER(__vstate);
}
} // namespace hana

#endif
