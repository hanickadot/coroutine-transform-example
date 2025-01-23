#ifndef GENERATOR_HPP
#define GENERATOR_HPP

#include "coroutines.hpp"

namespace hana {

template <typename T> struct generator {
	struct promise_type {
		T value;

		constexpr auto initial_suspend() const noexcept {
			return hana::suspend_always{};
		}

		constexpr auto final_suspend() const noexcept {
			return hana::suspend_always{};
		}

		constexpr auto get_return_object(hana::coroutine_handle<promise_type> handle) {
			return generator{handle};
		}

		constexpr auto yield_value(T v) noexcept {
			value = v;
			return hana::suspend_always{};
		}
	};

	hana::coroutine_handle<promise_type> handle;

	constexpr generator(hana::coroutine_handle<promise_type> hndl): handle{hndl} { }

	generator(const generator &) = delete;
	constexpr generator(generator && other) noexcept: handle{other.handle} {
		other.handle = nullptr;
	}

	generator & operator=(const generator &) = delete;
	constexpr generator & operator=(generator && other) noexcept {
		std::swap(handle, other.handle);
		return *this;
	}

	struct sentinel { };

	struct iterator {
		using difference_type = ptrdiff_t;
		using value_type = T;
		hana::coroutine_handle<promise_type> handle;

		constexpr friend bool operator==(iterator it, sentinel) noexcept {
			return it.handle.done();
		}

		constexpr friend bool operator==(iterator, iterator) noexcept;

		constexpr iterator(hana::coroutine_handle<promise_type> hndl): handle{hndl} {
			this->operator++();
		}

		constexpr value_type operator*() const noexcept {
			return handle.promise().value;
		}

		constexpr iterator & operator++() noexcept {
			handle.resume();
			return *this;
		}

		constexpr iterator operator++(int) noexcept;
	};

	static_assert(std::input_or_output_iterator<iterator>);
	static_assert(std::input_iterator<iterator>);

	constexpr auto begin() {
		return iterator{handle};
	}

	constexpr auto end() const noexcept {
		return sentinel{};
	}

	constexpr ~generator() {
		if (handle != nullptr) {
			handle.destroy();
		}
	}
};

} // namespace hana

#endif
