#include "generator.hpp"
#include "helpers.hpp"

struct __fib_state: hana::coroutine_promise_base<hana::generator<int>::promise_type> {
	// our promise type
	using promise_type = hana::generator<int>::promise_type;

	// for macros so they can cast to correct coroutine state (unique per coroutine/function)
	using self = __fib_state;

	// things which lives over suspension (temporaries)
	union {
		hana::return_type_of<&promise_type::initial_suspend> initial_awaiter;
		hana::return_type_of<&promise_type::yield_value> yield_awaiter;
	};

	// variables
	int a;
	int b;

	// states
	static constexpr void __coro_body(hana::coroutine_base * __vstate) {
		CORO_VAR(a) = 0; // int a = 0;
		CORO_VAR(b) = 1; // int b = 1;

		CORO_JUMP(__coro_loop);
	}

	static constexpr void __coro_loop(hana::coroutine_base * __vstate) {
		// co_yield b;
		std::construct_at(&CORO_VAR(yield_awaiter), CORO_VAR(__promise).yield_value(CORO_VAR(b)));

		// => co_await __promise.yield_value(b)
		CORO_AWAIT(CORO_VAR(yield_awaiter), __coro_resume_after_yield);
	}

	static constexpr void __coro_resume_after_yield(hana::coroutine_base * __vstate) {
		CORO_VAR(yield_awaiter).await_resume();
		std::destroy_at(&CORO_VAR(yield_awaiter));

		// a = std::exchange(b, a + b);
		CORO_VAR(a) = std::exchange(CORO_VAR(b), CORO_VAR(a) + CORO_VAR(b));

		if (CORO_VAR(b) > 6765) { // stop after 20 values, by jumping to final_suspend
			CORO_FINAL_SUSPEND();
		}

		CORO_JUMP(__coro_loop);
	}
};

constexpr auto fib() -> hana::generator<int> {
	return hana::wrapper<__fib_state>();
}

#include <array>
#include <iostream>
#include <ranges>

constexpr auto fib() -> hana::generator<int>;

consteval auto compile_time() {
	auto output = std::array<int, 30>{};
	auto it = output.begin();

	for (int v: fib()) {
		*it++ = v;
	}

	return output;
}

constexpr auto seq = compile_time();

constexpr auto check(int n) {
	return seq[n] == seq[n - 1] + seq[n - 2];
}

static_assert(seq[0] == 1);
static_assert(seq[1] == 1);
static_assert(check(2));
static_assert(check(3));
static_assert(check(4));
static_assert(check(5));
static_assert(check(6));
static_assert(check(7));
static_assert(check(8));
static_assert(check(9));
static_assert(check(10));
static_assert(check(11));
static_assert(check(12));
static_assert(check(13));
static_assert(check(14));
static_assert(check(15));
static_assert(check(16));
static_assert(check(17));
static_assert(check(18));
static_assert(seq[19] == 6765);

int main() {
	constexpr auto seq = compile_time();
	for (int v: seq) {
		std::cout << v << "\n";
	}
}