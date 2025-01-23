#ifndef HELPERS_HPP
#define HELPERS_HPP

namespace hana {

template <typename> struct return_value_helper_type;

template <typename R, typename Base, typename... Args, bool Noexcept> struct return_value_helper_type<R (Base::*)(Args...) const noexcept(Noexcept)> {
	using return_type = R;
};

template <typename R, typename Base, typename... Args, bool Noexcept> struct return_value_helper_type<R (Base::*)(Args...) noexcept(Noexcept)> {
	using return_type = R;
};

template <auto P> using return_type_of = return_value_helper_type<decltype(P)>::return_type;

} // namespace hana

#endif