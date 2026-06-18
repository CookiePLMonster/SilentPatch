#pragma once

// A set of convenience wrappers for accessing code or data from another module in a way that is compatible with other plugins patching references to those.
// Inspired by Link2012's injector::lazy_object.

// Usually, a simple 'type& Variable = *get_address("AA BB CC")' works, but a few possible compatibility issues arise:
// 1. When using Patterns, there is no way to cleanly indicate failure. Transactions can't be used, and non-transactional Patterns
//    won't give a clear failure state.
// 2. Other plugins may re-route the read variable after our plugin is done initializing, causing the plugin to reference stale data.
//    This problem is especially common with limit adjuster-esque mods relocating the arrays we may rely on.
// 3. x64 architectures use RIP-relative operands instead of absolute address, so the code needs to resolve the offset manually.

// For all wrappers from this file, the following rules apply:
// 1. The wrappers can be bound:
//    * On construction time with an address or a pattern.
//    * Lazily, using a .Bind method.
//    * By writing to a pointer returned from a .Put method.
// 2. Re-binding is permitted.
// 3. The results can later be checked with .Ensure().
// 4. The APIs are explicit, with no implicit conversions or call operators. This is to ensure that unintentional dependencies are difficult to create.
// 5. All wrappers are standard-layout types. This makes them usable inside inline assembly blocks, where they can be treated the same way the code
//    treated the underlying storage/function pointer types previously.

// ExternalRef<T> takes a pointer to an instruction operand (absolute address in x86, RIP-relative offset in x64) and can be used like a reference wrapper.
// When compiling for x64, an optional adjust offset is accepted by the constructors and .Bind methods, specifying how many bytes after the operand the instruction has.
// Each call to .Get() resolves and dereferences the operand, so changes made by any other plugins are immediately visible.
// Methods taking a 'stored_type* const* ptr' argument *technically* take an address of the operand, but an explicit type makes the intent clearer.
// This wrapper is the most useful when accessing global variables from the module - scalar types, arrays, objects, and so on.

// ExternalValue<T> takes an address of an immediate and exposes a reference to it, essentially working like std::reference_wrapper, but with a stricter API.
// Each call to .Get() re-reads the immediate.
// This wrapper is useful when you want to treat a constant immediate in the code as a variable your plugin can reference.
// Constness is enforced in the wrapper by returning the result of .Get() by value, and enforcing a const pointer type in .Address().

// ExternalFunc<Signature> takes an address of a function (call targets can't be resolved directly), and lets the user invoke that function with .Call(...).
// Different calling conventions are available.
// This wrapper is useful for referencing functions from the main module.

// ExternalMethod<Class, Signature> is equivalent to ExternalFunc, but for use with member function pointers.

// A helper EnsureBindings(...) function is available and can be used to check if all listed bindings were resolved.

#include "Utils/MemoryMgr.h"
#include "Utils/Patterns.h"

#include <cstddef>
#include <string_view>
#include <utility>

namespace external_bindings::details
{
	template<typename T>
	struct external_func_traits;

	template<typename R, typename... Args>
	struct external_func_traits<R __cdecl(Args...)>
	{
		using fnptr_type = R(__cdecl*)(Args...);
	};

	template<typename R, typename... Args>
	struct external_func_traits<R __stdcall(Args...)>
	{
		using fnptr_type = R(__stdcall*)(Args...);
	};

	template<typename R, typename... Args>
	struct external_func_traits<R __fastcall(Args...)>
	{
		using fnptr_type = R(__fastcall*)(Args...);
	};

	template<typename C, typename T>
	struct external_method_traits;

	template<typename C, typename R, typename... Args>
	struct external_method_traits<C, R(Args...)>
	{
		using fnptr_type = R(__thiscall*)(C*, Args...);
		using member_fnptr_type = R(C::*)(Args...);
		using classptr_type = C*;
	};

	template<typename C, typename R, typename... Args>
	struct external_method_traits<C, R(Args...) const>
	{
		using fnptr_type = R(__thiscall*)(const C*, Args...);
		using member_fnptr_type = R(C::*)(Args...) const;
		using classptr_type = const C*;
	};

	template<typename Derived, typename R, typename... Args>
	struct external_func_impl
	{
		R Call(Args... args) const
		{
			const Derived& self = static_cast<const Derived&>(*this);
			return self.Address()(std::move(args)...);
		}
	};

	template<typename Derived, typename Signature>
	struct external_func_base;

	template<typename Derived, typename R, typename... Args>
	struct external_func_base<Derived, R __cdecl(Args...)> : public external_func_impl<Derived, R, Args...>
	{
	};

	template<typename Derived, typename R, typename... Args>
	struct external_func_base<Derived, R __stdcall(Args...)> : public external_func_impl<Derived, R, Args...>
	{
	};

	template<typename Derived, typename R, typename... Args>
	struct external_func_base<Derived, R __fastcall(Args...)> : public external_func_impl<Derived, R, Args...>
	{
	};

	template<typename Derived, typename C, typename R, typename... Args>
	struct external_method_impl
	{
		R Call(C* obj, Args... args) const
		{
			const Derived& self = static_cast<const Derived&>(*this);
			return self.Address()(obj, std::move(args)...);
		}
	};

	template<typename Derived, typename C, typename Signature>
	struct external_method_base;

	template<typename Derived, typename C, typename R, typename... Args>
	struct external_method_base<Derived, C, R(Args...)> : public external_method_impl<Derived, C, R, Args...>
	{
	};

	template<typename Derived, typename C, typename R, typename... Args>
	struct external_method_base<Derived, C, R(Args...) const> : public external_method_impl<Derived, const C, R, Args...>
	{
	};

	inline const void* try_get_pattern(std::string_view pattern_string, std::ptrdiff_t offset)
	{
		hook::pattern pattern(pattern_string);
		if (pattern.count_hint(1).size() == 1)
		{
			return pattern.get_first<const void>(offset);
		}
		return nullptr;
	}
}

template<typename T>
class ExternalRef
{
public:
	using stored_type = T;

	ExternalRef() noexcept = default;

	explicit ExternalRef(stored_type* const* ptr
#ifdef _M_X64
		, std::ptrdiff_t adjust = 0
#endif
	) noexcept
		: m_operand_ptr(ptr)
#ifdef _M_X64
		, m_adjust(adjust)
#endif
	{
	}

	explicit ExternalRef(std::string_view pattern_string, std::ptrdiff_t offset = 0
#ifdef _M_X64
		, std::ptrdiff_t adjust = 0
#endif
	)
		: ExternalRef(static_cast<stored_type* const*>(external_bindings::details::try_get_pattern(pattern_string, offset))
#ifdef _M_X64
		, adjust
#endif
		)
	{
	}

	void Bind(stored_type* const* ptr
#ifdef _M_X64
		, std::ptrdiff_t adjust = 0
#endif
	) noexcept
	{
		m_operand_ptr = ptr;
#ifdef _M_X64
		m_adjust = adjust;
#endif
	}

	void Bind(std::string_view pattern_string, std::ptrdiff_t offset = 0
#ifdef _M_X64
		, std::ptrdiff_t adjust = 0
#endif
	)
	{
		Bind(static_cast<stored_type* const*>(external_bindings::details::try_get_pattern(pattern_string, offset))
#ifdef _M_X64
		, adjust
#endif
		);
	}

	[[nodiscard]] stored_type& Get() const
	{
		return *Address();
	}

	[[nodiscard]] stored_type* Address() const
	{
		stored_type* ptr;
		Memory::ReadMemDisplacement(m_operand_ptr, ptr, m_adjust);
		return ptr;
	}

	// Rarely needed, but may be useful for hooking
	const void* OperandAddress() const noexcept
	{
		return m_operand_ptr;
	}

	[[nodiscard]] const void** Put(
#ifdef _M_X64
		std::ptrdiff_t adjust = 0
#endif
	) noexcept
	{
		m_operand_ptr = nullptr;
#ifdef _M_X64
		m_adjust = adjust;
#endif
		return &m_operand_ptr;
	}

	[[nodiscard]] bool Ensure() const noexcept
	{
		return m_operand_ptr != nullptr;
	}

	// Copy construction and assignment almost certainly indicates user error
	ExternalRef(const ExternalRef&) = delete;
	ExternalRef& operator=(const ExternalRef&) = delete;

private:
	const void* m_operand_ptr = nullptr;
#ifdef _M_X64
	ptrdiff_t m_adjust = 0;
#else
	static constexpr std::ptrdiff_t m_adjust = 0; // Adjust is not needed for 32-bit code
#endif
};

template<typename T>
class ExternalValue
{
public:
	using stored_type = T;

	ExternalValue() noexcept = default;

	explicit ExternalValue(stored_type const* ptr) noexcept
		: m_ptr(ptr)
	{
	}

	explicit ExternalValue(std::string_view pattern_string, std::ptrdiff_t offset = 0)
		: ExternalValue(static_cast<stored_type const*>(external_bindings::details::try_get_pattern(pattern_string, offset)))
	{
	}

	void Bind(stored_type const* ptr) noexcept { m_ptr = ptr; }

	void Bind(std::string_view pattern_string, std::ptrdiff_t offset = 0)
	{
		Bind(static_cast<stored_type const*>(external_bindings::details::try_get_pattern(pattern_string, offset)));
	}

	[[nodiscard]] stored_type Get() const
	{
		return *Address();
	}

	[[nodiscard]] stored_type const* Address() const noexcept
	{
		return m_ptr;
	}

	[[nodiscard]] stored_type const** Put() noexcept
	{
		m_ptr = nullptr;
		return &m_ptr;
	}

	[[nodiscard]] bool Ensure() const noexcept
	{
		return m_ptr != nullptr;
	}

	// Copy construction and assignment almost certainly indicates user error
	ExternalValue(const ExternalValue&) = delete;
	ExternalValue& operator=(const ExternalValue&) = delete;

private:
	stored_type const* m_ptr = nullptr;
};

template<typename Signature>
class ExternalFunc : public external_bindings::details::external_func_base<ExternalFunc<Signature>, Signature>
{
	using traits = external_bindings::details::external_func_traits<Signature>;

public:
	using fnptr_type = typename traits::fnptr_type;

	ExternalFunc() noexcept = default;

	explicit ExternalFunc(fnptr_type func) noexcept
		: m_func(func)
	{
	}

	explicit ExternalFunc(std::string_view pattern_string, std::ptrdiff_t offset = 0)
		: ExternalFunc(reinterpret_cast<fnptr_type>(external_bindings::details::try_get_pattern(pattern_string, offset)))
	{
	}

	void Bind(fnptr_type func) noexcept { m_func = func; }

	void Bind(std::string_view pattern_string, std::ptrdiff_t offset = 0)
	{
		Bind(reinterpret_cast<fnptr_type>(external_bindings::details::try_get_pattern(pattern_string, offset)));
	}

	[[nodiscard]] fnptr_type Address() const noexcept
	{
		return m_func;
	}

	[[nodiscard]] fnptr_type* Put() noexcept
	{
		m_func = nullptr;
		return &m_func;
	}

	[[nodiscard]] bool Ensure() const noexcept
	{
		return m_func != nullptr;
	}

	// Copy construction and assignment almost certainly indicates user error
	ExternalFunc(const ExternalFunc&) = delete;
	ExternalFunc& operator=(const ExternalFunc&) = delete;

private:
	fnptr_type m_func = nullptr;
};

template<typename Class, typename Signature>
class ExternalMethod : public external_bindings::details::external_method_base<ExternalMethod<Class, Signature>, Class, Signature>
{
	using traits = external_bindings::details::external_method_traits<Class, Signature>;

public:
	using fnptr_type = typename traits::fnptr_type;
	using member_fnptr_type = typename traits::member_fnptr_type;
	using classptr_type = typename traits::classptr_type;

	ExternalMethod() noexcept = default;

	explicit ExternalMethod(fnptr_type func) noexcept
		: m_func(func)
	{
	}

	explicit ExternalMethod(member_fnptr_type func) noexcept
		: ExternalMethod(to_fnptr(func))
	{
	}

	explicit ExternalMethod(std::string_view pattern_string, std::ptrdiff_t offset = 0)
		: ExternalMethod(reinterpret_cast<fnptr_type>(external_bindings::details::try_get_pattern(pattern_string, offset)))
	{
	}

	void Bind(fnptr_type func) noexcept { m_func = func; }
	void Bind(member_fnptr_type func) noexcept { Bind(to_fnptr(func)); }

	void Bind(std::string_view pattern_string, std::ptrdiff_t offset = 0)
	{
		Bind(reinterpret_cast<fnptr_type>(external_bindings::details::try_get_pattern(pattern_string, offset)));
	}

	[[nodiscard]] fnptr_type Address() const noexcept
	{
		return m_func;
	}

	[[nodiscard]] fnptr_type* Put() noexcept
	{
		m_func = nullptr;
		return &m_func;
	}

	[[nodiscard]] bool Ensure() const noexcept
	{
		return m_func != nullptr;
	}

	// Copy construction and assignment almost certainly indicates user error
	ExternalMethod(const ExternalMethod&) = delete;
	ExternalMethod& operator=(const ExternalMethod&) = delete;

private:
	static fnptr_type to_fnptr(member_fnptr_type func)
	{
		fnptr_type result;
		memcpy(&result, &func, sizeof(result));
		return result;
	}

private:
	fnptr_type m_func = nullptr;
};


// ExternalFunc and ExternalMethod need deduction guides
template<typename R, typename... Args>
ExternalFunc(R(__cdecl*)(Args...)) -> ExternalFunc<R __cdecl(Args...)>;

template<typename R, typename... Args>
ExternalFunc(R(__stdcall*)(Args...)) -> ExternalFunc<R __stdcall(Args...)>;

template<typename R, typename... Args>
ExternalFunc(R(__fastcall*)(Args...)) -> ExternalFunc<R __fastcall(Args...)>;

template<typename C, typename R, typename... Args>
ExternalMethod(R(__thiscall*)(C*, Args...)) -> ExternalMethod<C, R(Args...)>;

template<typename C, typename R, typename... Args>
ExternalMethod(R(__thiscall*)(const C*, Args...)) -> ExternalMethod<C, R(Args...) const>;

template<typename... Bindings>
[[nodiscard]] bool EnsureBindings(const Bindings&... bindings)
{
	return (bindings.Ensure() && ...);
}
