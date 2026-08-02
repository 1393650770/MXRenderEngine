#pragma once
#ifndef _CONSTDEFINE_
#define _CONSTDEFINE_

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <string>
#include <queue>
#include <stack>
#include <array>
#include <stdexcept>
#ifdef _MSC_VER
#define FORCEINLINE __forceinline
#define __forceinline __forceinline
#else
#define FORCEINLINE __attribute__((always_inline)) inline
#define __forceinline __attribute__((always_inline)) inline
#endif

#if !PLATFORM_ANDROID && !PLATFORM_WGPU && !PLATFORM_GLES3
#include <boost/stacktrace.hpp>
#endif
#include <iostream>
#include <memory>
#include "Reflection.h"
#ifndef MYRENDERER_C_INTERFACE
#    ifdef __cplusplus
#        define MYRENDERER_C_INTERFACE 0
#    else
#        define MYRENDERER_C_INTERFACE 1
#    endif
#endif

#ifdef _MSC_VER
#    define MYRENDERER_CALL_TYPE __cdecl
#else
#    define MYRENDERER_CALL_TYPE
#endif

#if UINTPTR_MAX == UINT64_MAX
#    define MYRENDERER_PLATFORM_64 1
#elif UINTPTR_MAX == UINT32_MAX
#    define MYRENDERER_PLATFORM_32 1
#else
#    pragma error Unexpected value of UINTPTR_MAX
#endif


#define MYRENDERER_BEGIN_NAMESPACE(Name) \
        namespace Name                 \
        {

#define MYRENDERER_END_NAMESPACE }

#define MYRENDERER_PUBLIC_DERIVE(TypeName)  public TypeName 
        

#define MYRENDERER_BEGIN_TYPED_ENUM(EnumName, EnumType) enum EnumName : EnumType \
                                {
#define MYRENDERER_END_TYPED_ENUM };

#define MYRENDERER_INITIALIZER(x) = x

#define MYRENDERER_GLOBAL_FUNCTION(FuncName) FuncName

#if defined(__REFLECTION_PARSER__)
#define META(...) __attribute__((annotate(#__VA_ARGS__)))
#define MYRENDERER_BEGIN_STRUCT(Name, ...) struct __attribute__((annotate(#__VA_ARGS__))) Name  \
                                    {
#define MYRENDERER_BEGIN_CLASS(Name, ...) class __attribute__((annotate(#__VA_ARGS__))) Name  \
                                    {
#define MYRENDERER_BEGIN_CLASS_WITH_DERIVE(Name,BaseClass, ...) class __attribute__((annotate(#__VA_ARGS__))) Name : BaseClass \
                                        {
#else
#define META(...)
#define MYRENDERER_BEGIN_STRUCT(Name, ...) struct Name  \
                                    {
#define MYRENDERER_BEGIN_CLASS(Name, ...) class Name  \
                                    { 
#define MYRENDERER_BEGIN_CLASS_WITH_DERIVE(Name,BaseClass, ...) class Name : BaseClass \
                                        {
#endif
#define MYRENDERER_END_CLASS  };

#define MYRENDERER_END_STRUCT };

#define MYRENDERER_TEMPLATE_HEAD(...)  template<__VA_ARGS__>

#define MYRENDERER_VALUE(x) = x

#define CHECK(Flag) if(Flag) { std::abort(); }
#if PLATFORM_ANDROID
// Android: Boost not available, use logcat for diagnostics (UE pattern — never silent-fail)
// Uses %s format — all callers must pass a single string (literal, c_str(), or pre-formatted)
#include <android/log.h>
#define CHECK_WITH_LOG(Flag, ...) if(Flag) { __android_log_print(ANDROID_LOG_ERROR, "MXRender", "CHECK: %s", __VA_ARGS__); throw std::runtime_error("CHECK failed"); }
#define CHECK_WITH_LOG_WARNING(Flag, ...) if(Flag) { __android_log_print(ANDROID_LOG_WARN, "MXRender", "CHECK: %s", __VA_ARGS__); }
#elif PLATFORM_WGPU || PLATFORM_GLES3
// WebGPU/GLES3 wasm: Boost not available, use cout for diagnostics (UE pattern — never silent-fail)
#define CHECK_WITH_LOG(Flag,LOG) if(Flag) \
                                {\
                                    std::cout<<"CHECK: "<<String(LOG)<<std::endl; \
                                    throw std::runtime_error(String(LOG)); \
                                }
#define CHECK_WITH_LOG_WARNING(Flag,LOG) if(Flag) \
                                {\
                                    std::cout<<"CHECK: "<<String(LOG)<<std::endl; \
                                }
#else
#define CHECK_WITH_LOG(Flag,LOG) if(Flag) \
                                {\
                                    boost::stacktrace::stacktrace stack_trace;\
									String log = String(LOG)+"\n"+boost::stacktrace::to_string(stack_trace);\
									std::cout<<log<<std::endl; \
                                   	throw std::runtime_error(String(LOG)+"\n"+boost::stacktrace::to_string(stack_trace)); \
                                }
#define CHECK_WITH_LOG_WARNING(Flag,LOG) if(Flag) \
                                {\
                                    boost::stacktrace::stacktrace stack_trace;\
                                   	std::cout<< String(LOG)+"\n"+boost::stacktrace::to_string(stack_trace) <<std::endl; \
                                }
#endif
// Positive-semantics assertions (UE ENSURE naming): fire when the condition is
// FALSE - complementary to the inverted CHECK family. New code should use
// ENSURE; the CHECK family keeps its inverted semantics for existing callers.
#define ENSURE(Condition, ...) CHECK_WITH_LOG(!(Condition), __VA_ARGS__)
#define ENSURE_WITH_LOG(Condition, ...) CHECK_WITH_LOG(!(Condition), __VA_ARGS__)

#define VIRTUAL      virtual
#define CONST        const
#define MYDEFAULT      =default
#define PURE         = 0

#define MYDELETE       = delete

#define OVERRIDE      override
#define FINAL        final
#define REF          &
#define METHOD(Name) MYRENDERER_CALL_TYPE Name

#define STATIC_CAST(Pointer,Type) (static_cast<Type*>(Pointer))
#define DYNAMIC_CAST(Pointer,Type) (dynamic_cast<Type>(Pointer))
#define REINTERPRET_CAST(Pointer,Type) (reinterpret_cast<Type*>(Pointer))
#define CONST_CAST(Pointer,Type) (const_cast<Type>(Pointer))
using UInt8 = std::uint8_t;
using UInt16 = std::uint16_t;
using UInt32 = std::uint32_t;
using UInt64 = std::uint64_t;
using Int8 = std::int8_t;
using Int16 = std::int16_t;
using Int = std::int32_t;
using Int64 = std::int64_t;
using Bool = bool;
using Char = char;
using String = std::string;
using Text = std::string;
using Float32 = float;
using Float64 = double;

template <typename T>
using Vector = std::vector<T>; 

template <typename K,typename T, typename Hash = std::hash<K>>
using Map = std::unordered_map<K,T, Hash>;

template <typename T>
using Optional = std::optional<T>;

template <typename T>
using Set = std::unordered_set<T>;

template <typename T>
using Queue =std::queue<T>;

template <typename T>
using Stack = std::stack<T>;

template <class T, size_t Size>
using Array = std::array<T, Size>;

template <class T, class FuncDelete = std::default_delete<T> >
using UniquePtr = std::unique_ptr<T, FuncDelete>;

#ifdef _MSC_VER
#define DLLEXPORT __declspec(dllexport)
#define DLLIMPORT __declspec(dllimport)
#else
#define DLLEXPORT
#define DLLIMPORT
#endif

#define CORE_API DLLEXPORT
#define TRACELOG_API DLLIMPORT
#define DESKTOPPLATFORM_API DLLIMPORT
#define ANALYTICS_API DLLIMPORT
#define DIRECTORYWATCHER_API DLLIMPORT


#define ENUM_CLASS_FLAGS(Enum) \
    extern "C++" \
	{				\
	inline           Enum& operator|=(Enum& Lhs, Enum Rhs) { return Lhs = (Enum)((__underlying_type(Enum))Lhs | (__underlying_type(Enum))Rhs); } \
	inline           Enum& operator&=(Enum& Lhs, Enum Rhs) { return Lhs = (Enum)((__underlying_type(Enum))Lhs & (__underlying_type(Enum))Rhs); } \
	inline           Enum& operator^=(Enum& Lhs, Enum Rhs) { return Lhs = (Enum)((__underlying_type(Enum))Lhs ^ (__underlying_type(Enum))Rhs); } \
	inline constexpr Enum  operator| (Enum  Lhs, Enum Rhs) { return (Enum)((__underlying_type(Enum))Lhs | (__underlying_type(Enum))Rhs); } \
	inline constexpr Enum  operator& (Enum  Lhs, Enum Rhs) { return (Enum)((__underlying_type(Enum))Lhs & (__underlying_type(Enum))Rhs); } \
	inline constexpr Enum  operator^ (Enum  Lhs, Enum Rhs) { return (Enum)((__underlying_type(Enum))Lhs ^ (__underlying_type(Enum))Rhs); } \
	inline constexpr Bool  operator! (Enum  E)             { return !(__underlying_type(Enum))E; } \
	inline constexpr Enum  operator~ (Enum  E)             { return (Enum)~(__underlying_type(Enum))E; }\
	}

// Friends all bitwise operators for enum classes so the definition can be kept private / protected.
#define FRIEND_ENUM_CLASS_FLAGS(Enum) \
	friend           Enum& operator|=(Enum& Lhs, Enum Rhs); \
	friend           Enum& operator&=(Enum& Lhs, Enum Rhs); \
	friend           Enum& operator^=(Enum& Lhs, Enum Rhs); \
	friend constexpr Enum  operator| (Enum  Lhs, Enum Rhs); \
	friend constexpr Enum  operator& (Enum  Lhs, Enum Rhs); \
	friend constexpr Enum  operator^ (Enum  Lhs, Enum Rhs); \
	friend constexpr bool  operator! (Enum  E); \
	friend constexpr Enum  operator~ (Enum  E);

template<typename Enum>
constexpr bool EnumHasAllFlags(Enum Flags, Enum Contains)
{
	using UnderlyingType = __underlying_type(Enum);
	return ((UnderlyingType)Flags & (UnderlyingType)Contains) == (UnderlyingType)Contains;
};

template<typename Enum>
constexpr bool EnumHasAnyFlags(Enum Flags, Enum Contains)
{
	using UnderlyingType = __underlying_type(Enum);
	return ((UnderlyingType)Flags & (UnderlyingType)Contains) != 0;
};

template<typename Enum>
void EnumAddFlags(Enum & Flags, Enum FlagsToAdd)
{
	using UnderlyingType = __underlying_type(Enum);
	Flags = (Enum)((UnderlyingType)Flags | (UnderlyingType)FlagsToAdd);
};

template<typename Enum>
void EnumRemoveFlags(Enum& Flags, Enum FlagsToRemove)
{
	using UnderlyingType = __underlying_type(Enum);
	Flags = (Enum)((UnderlyingType)Flags & ~(UnderlyingType)FlagsToRemove);
};


template <typename T>
struct TIsIntegral
{
	enum { Value = false };
};
template <> struct TIsIntegral<         bool> { enum { Value = true }; };
template <> struct TIsIntegral<         char> { enum { Value = true }; };
template <> struct TIsIntegral<signed   char> { enum { Value = true }; };
template <> struct TIsIntegral<unsigned char> { enum { Value = true }; };
template <> struct TIsIntegral<         char16_t> { enum { Value = true }; };
template <> struct TIsIntegral<         char32_t> { enum { Value = true }; };
template <> struct TIsIntegral<         wchar_t> { enum { Value = true }; };
template <> struct TIsIntegral<         short> { enum { Value = true }; };
template <> struct TIsIntegral<unsigned short> { enum { Value = true }; };
template <> struct TIsIntegral<         int> { enum { Value = true }; };
template <> struct TIsIntegral<unsigned int> { enum { Value = true }; };
template <> struct TIsIntegral<         long> { enum { Value = true }; };
template <> struct TIsIntegral<unsigned long> { enum { Value = true }; };
template <> struct TIsIntegral<         long long> { enum { Value = true }; };
template <> struct TIsIntegral<unsigned long long> { enum { Value = true }; };
template <typename T> struct TIsIntegral<const          T> { enum { Value = TIsIntegral<T>::Value }; };
template <typename T> struct TIsIntegral<      volatile T> { enum { Value = TIsIntegral<T>::Value }; };
template <typename T> struct TIsIntegral<const volatile T> { enum { Value = TIsIntegral<T>::Value }; };



template <typename T>
struct TIsPointer
{
	enum { Value = false };
};
template <typename T> struct TIsPointer<T*> { enum { Value = true }; };
template <typename T> struct TIsPointer<const          T> { enum { Value = TIsPointer<T>::Value }; };
template <typename T> struct TIsPointer<      volatile T> { enum { Value = TIsPointer<T>::Value }; };
template <typename T> struct TIsPointer<const volatile T> { enum { Value = TIsPointer<T>::Value }; };

template <typename T>
FORCEINLINE constexpr T Align(T Val, UInt64 Alignment)
{
	static_assert(TIsIntegral<T>::Value || TIsPointer<T>::Value, "Align expects an integer or pointer type");

	return (T)(((UInt64)Val + Alignment - 1) & ~(Alignment - 1));
};

template <bool AllowTouch, typename T>
bool CheckLineSectionOverlap(T Min0, T Max0, T Min1, T Max1)
{

	//     [------]         [------]
	//   Min0    Max0    Min1     Max1
	//
	//     [------]         [------]
	//   Min1    Max1    Min0     Max0
	if (AllowTouch)
	{
		return !(Min0 > Max1 || Min1 > Max0);
	}
	else
	{
		return !(Min0 >= Max1 || Min1 >= Max0);
	}
};
#endif 
