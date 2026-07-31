#pragma once
#ifndef _GAMEPLAY_EVENT_BUS_
#define _GAMEPLAY_EVENT_BUS_

#include "Core/ConstDefine.h"
#include <functional>
#include <typeindex>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// Type-erased channel base (keeps the registry homogeneous).
class IEventChannel
{
public:
	VIRTUAL ~IEventChannel() MYDEFAULT;
	VIRTUAL void METHOD(Clear)() PURE;
};

// Typed observer channel. Subscribe returns a token (not a raw this pointer);
// handlers must Unsubscribe before destruction or rely on ClearAll at world
// shutdown. Publish copies the entry list before iterating (safe against
// subscribe/unsubscribe during dispatch).
template<typename T>
class EventChannel : public IEventChannel
{
public:
	using Token = UInt32;

	Token METHOD(Subscribe)(std::function<void(const T&)> handler)
	{
		Token token = ++next_token_;
		entries_.push_back({ token, std::move(handler) });
		return token;
	}

	void METHOD(Unsubscribe)(Token token)
	{
		for (auto it = entries_.begin(); it != entries_.end(); ++it)
		{
			if (it->token == token)
			{
				entries_.erase(it);
				return;
			}
		}
	}

	void METHOD(Publish)(CONST T& evt)
	{
		Vector<Entry> snapshot = entries_;   // copy before iterating
		for (CONST auto& entry : snapshot)
		{
			if (entry.handler)
				entry.handler(evt);
		}
	}

	VIRTUAL void METHOD(Clear)() OVERRIDE { entries_.clear(); }

private:
	struct Entry
	{
		Token token = 0;
		std::function<void(const T&)> handler;
	};
	Vector<Entry> entries_;
	Token next_token_ = 1;
};

// Typed observer facade for gameplay events (damage, death, explosion...).
// GameWorld owns one instance; systems subscribe via template methods.
MYRENDERER_BEGIN_CLASS(GameplayEventBus)
#pragma region METHOD
public:
	GameplayEventBus() MYDEFAULT;
	~GameplayEventBus() MYDEFAULT;

	template<typename T>
	typename EventChannel<T>::Token METHOD(Subscribe)(std::function<void(const T&)> handler)
	{
		auto& channel = GetChannel<T>();
		return channel.Subscribe(std::move(handler));
	}

	template<typename T>
	void METHOD(Unsubscribe)(typename EventChannel<T>::Token token)
	{
		GetChannel<T>().Unsubscribe(token);
	}

	template<typename T>
	void METHOD(Publish)(CONST T& evt)
	{
		GetChannel<T>().Publish(evt);
	}

	// Only called by GameWorld::Shutdown (not Reset - tokens stay valid
	// across resets).
	void METHOD(ClearAll)();

protected:

private:
	template<typename T>
	EventChannel<T>& METHOD(GetChannel)()
	{
		auto it = channels_.find(std::type_index(typeid(T)));
		if (it == channels_.end())
		{
			auto channel = std::make_unique<EventChannel<T>>();
			it = channels_.emplace(std::type_index(typeid(T)), std::move(channel)).first;
		}
		return *static_cast<EventChannel<T>*>(it->second.get());
	}

#pragma endregion

#pragma region MEMBER
public:

protected:
	Map<std::type_index, UniquePtr<IEventChannel>> channels_;

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _GAMEPLAY_EVENT_BUS_