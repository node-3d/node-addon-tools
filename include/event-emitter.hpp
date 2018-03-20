#ifndef _EVENT_EMITTER_
#define _EVENT_EMITTER_


#include <addon-tools.hpp>

#include <map>
#include <deque>
#include <iostream> // -> std::cout << "..." << std::endl;


#define THIS_EVENT_EMITTER                                                    \
	EventEmitter *eventEmitter = ObjectWrap::Unwrap<EventEmitter>(info.This());

#define EVENT_EMITTER_THIS_CHECK                                              \
	if (eventEmitter->_isDestroyed) return;

#define EVENT_EMITTER_DES_CHECK                                               \
	if (_isDestroyed) return;


template <typename T>
struct StaticHolder {
	static Nan::Persistent<v8::FunctionTemplate> _prototype;
	static Nan::Persistent<v8::Function> _constructor;
};

template <typename T> Nan::Persistent<v8::FunctionTemplate> StaticHolder<T>::_prototype;
template <typename T> Nan::Persistent<v8::Function> StaticHolder<T>::_constructor;


class EventEmitter : public StaticHolder<int>, public Nan::ObjectWrap {
	
	typedef Nan::CopyablePersistentTraits<v8::Function>::CopyablePersistent FN_TYPE;
	typedef std::deque<FN_TYPE> VEC_TYPE;
	typedef std::map<std::string, VEC_TYPE> MAP_TYPE;
	typedef std::map<int, FN_TYPE> FNMAP_TYPE;
	typedef VEC_TYPE::iterator IT_TYPE;
	typedef MAP_TYPE::iterator MAP_IT_TYPE;
	typedef FNMAP_TYPE::iterator FNMAP_IT_TYPE;
	
// Public V8 init
public:
	
	static void init(v8::Local<v8::Object> target) {
		
		v8::Local<v8::FunctionTemplate> proto = Nan::New<v8::FunctionTemplate>(newCtor);
		
		proto->InstanceTemplate()->SetInternalFieldCount(1);
		proto->SetClassName(JS_STR("EventEmitter"));
		
		
		// Accessors
		v8::Local<v8::ObjectTemplate> obj = proto->PrototypeTemplate();
		ACCESSOR_R(obj, isDestroyed);
		
		
		// -------- dynamic
		
		Nan::SetPrototypeMethod(proto, "listenerCount", jsListenerCount);
		Nan::SetPrototypeMethod(proto, "addListener", jsAddListener);
		Nan::SetPrototypeMethod(proto, "emit", jsEmit);
		Nan::SetPrototypeMethod(proto, "eventNames", jsEventNames);
		Nan::SetPrototypeMethod(proto, "getMaxListeners", jsGetMaxListeners);
		Nan::SetPrototypeMethod(proto, "listeners", jsListeners);
		Nan::SetPrototypeMethod(proto, "on", jsOn);
		Nan::SetPrototypeMethod(proto, "once", jsOnce);
		Nan::SetPrototypeMethod(proto, "prependListener", jsPrependListener);
		Nan::SetPrototypeMethod(proto, "prependOnceListener", jsPrependOnceListener);
		Nan::SetPrototypeMethod(proto, "removeAllListeners", jsRemoveAllListeners);
		Nan::SetPrototypeMethod(proto, "removeListener", jsRemoveListener);
		Nan::SetPrototypeMethod(proto, "setMaxListeners", jsSetMaxListeners);
		Nan::SetPrototypeMethod(proto, "rawListeners", jsRawListeners);
		
		// -------- static
		
		v8::Local<v8::Function> ctor = Nan::GetFunction(proto).ToLocalChecked();
		
		v8::Local<v8::Object> ctorObj = v8::Local<v8::Object>::Cast(ctor);
		
		Nan::SetMethod(ctorObj, "listenerCount", jsStaticListenerCount);
		
		
		_constructor.Reset(ctor);
		_prototype.Reset(proto);
		
		Nan::Set(target, JS_STR("EventEmitter"), ctor);
		
	}
	
	
	// C++ side emit() method
	void emit(const std::string &name, int argc = 0, v8::Local<v8::Value> *argv = NULL) {
		
		// Important! As actual get map[key] produces a new (empty) map entry
		if ( _listeners.find(name) == _listeners.end() ) {
			return;
		}
		
		// A copy is intended, because handlers can call removeListener (and they DO)
		VEC_TYPE list = _listeners[name];
		
		if (list.empty()) {
			return;
		}
		
		for (IT_TYPE it = list.begin(); it != list.end(); ++it) {
			
			Nan::Callback callback(Nan::New(*it));
			
			if ( ! callback.IsEmpty() ) {
				callback.Call(argc, argv);
			}
			
		}
		
	}
	
	
	// C++ side on() method
	void on(const std::string &name, v8::Local<v8::Value> that, const std::string &method) {
		
		v8::Local<v8::String> code = JS_STR(
			"((emitter, name, that, method) => emitter.on(name, that[method]))"
		);
		
		v8::Local<v8::Function> connector = v8::Local<v8::Function>::Cast(v8::Script::Compile(code)->Run());
		Nan::Callback connectorCb(connector);
		
		v8::Local<v8::Object> emitter = Nan::New<v8::Object>();
		this->Wrap(emitter);
		
		v8::Local<v8::Value> argv[] = { emitter, JS_STR(name.c_str()), that, JS_STR(method.c_str()) };
		connectorCb.Call(4, argv);
		
	}
	
	
	void _destroy() { EVENT_EMITTER_DES_CHECK;
		_isDestroyed = true;
		emit("destroy");
	}
	
	
private:
	
	EventEmitter () {
		_isDestroyed = false;
		_maxListeners = 0;
		_freeId = 0;
	}
	
	virtual ~EventEmitter () { _destroy(); }
	
	
	static NAN_METHOD(newCtor) {
		std::cout << "EventEmitter() 1" << std::endl;
		EventEmitter *eventEmitter = new EventEmitter();
		eventEmitter->Wrap(info.This());
		std::cout << "EventEmitter() 2 : @" << eventEmitter << std::endl;
		RET_VALUE(info.This());
		
	}
	
	
	static NAN_GETTER(isDestroyedGetter) { THIS_EVENT_EMITTER;
		
		RET_VALUE(JS_BOOL(eventEmitter->_isDestroyed));
		
	}
	
	
	NAN_METHOD(destroy) { THIS_EVENT_EMITTER; EVENT_EMITTER_THIS_CHECK;
		
		eventEmitter->_destroy();
		
	}
	
	
	// Deprecated static method
	static NAN_METHOD(jsStaticListenerCount) {
		
		REQ_OBJ_ARG(0, obj);
		EventEmitter *eventEmitter = ObjectWrap::Unwrap<EventEmitter>(obj);
		REQ_UTF8_ARG(1, name);
		
		const VEC_TYPE &list = eventEmitter->_listeners[*name];
		
		RET_VALUE(JS_INT(static_cast<int>(list.size())));
		
	}
	
	
	static NAN_METHOD(jsAddListener) { _wrapListener(info); }
	
	
	static NAN_METHOD(jsEmit) { THIS_EVENT_EMITTER;
		
		REQ_UTF8_ARG(0, name);
		
		int length = info.Length();
		
		std::vector< v8::Local<v8::Value> > args;
		
		for (int i = 1; i < length; i++) {
			args.push_back(info[i]);
		}
		
		eventEmitter->emit(*name, length - 1, &args[0]);
		
	}
	
	
	static NAN_METHOD(jsEventNames) { THIS_EVENT_EMITTER;
		
		std::cout << "jsEventNames() 1: @" << eventEmitter << " x " << eventEmitter->_raw.size() << std::endl;
		
		v8::Local<v8::Array> jsNames = Nan::New<v8::Array>(eventEmitter->_raw.size());
		std::cout << "jsEventNames() 2" << std::endl;
		if (eventEmitter->_raw.empty()) {
			RET_VALUE(jsNames);
			return;
		}
		
		int i = 0;
		for (MAP_IT_TYPE it = eventEmitter->_raw.begin(); it != eventEmitter->_raw.end(); ++it, i++) {
			
			jsNames->Set(JS_INT(i), JS_STR(it->first));
			
		}
		
		RET_VALUE(jsNames);
		
	}
	
	
	static NAN_METHOD(jsGetMaxListeners) { THIS_EVENT_EMITTER;
		
		RET_VALUE(JS_INT(eventEmitter->_maxListeners));
		
	}
	
	
	static NAN_METHOD(jsListenerCount) { THIS_EVENT_EMITTER;
		
		REQ_UTF8_ARG(0, name);
		
		const VEC_TYPE &list = eventEmitter->_listeners[*name];
		
		RET_VALUE(JS_INT(static_cast<int>(list.size())));
		
	}
	
	
	static NAN_METHOD(jsListeners) { THIS_EVENT_EMITTER;
		
		REQ_UTF8_ARG(0, name);
		
		VEC_TYPE &list = eventEmitter->_listeners[*name];
		
		v8::Local<v8::Array> jsListeners = Nan::New<v8::Array>(list.size());
		
		if (list.empty()) {
			RET_VALUE(jsListeners);
			return;
		}
		
		int i = 0;
		for (IT_TYPE it = list.begin(); it != list.end(); ++it, i++) {
			
			jsListeners->Set(JS_INT(i), Nan::New(*it));
			
		}
		
		RET_VALUE(jsListeners);
		
	}
	
	
	static inline void _addListener(
		const Nan::FunctionCallbackInfo<v8::Value> &info,
		const std::string &name,
		Nan::Persistent<v8::Function> &cb,
		bool isFront
	) { THIS_EVENT_EMITTER;
		
		v8::Local<v8::Value> args[] = { info[0], info[1] };
		eventEmitter->emit("newListener", 2, args);
		
		if (isFront) {
			eventEmitter->_listeners[name].push_front(cb);
			eventEmitter->_raw[name].push_front(cb);
		} else {
			eventEmitter->_listeners[name].push_back(cb);
			eventEmitter->_raw[name].push_back(cb);
		}
		
		int count = eventEmitter->_raw[name].size();
		
		if (eventEmitter->_maxListeners > 0 && count > eventEmitter->_maxListeners) {
			
			std::cout << "EventEmitter Warning: too many listeners (";
			std::cout << count << " > " << eventEmitter->_maxListeners << ") on '";
			std::cout << name << "' event, possible memory leak." << std::endl;
			
			// Some JS magic to retrieve the call stack
			v8::Local<v8::String> code = JS_STR(
				"(new Error()).stack.split('\\n').slice(1).join('\\n')"
			);
			v8::Local<v8::String> stack = v8::Local<v8::String>::Cast(
				v8::Script::Compile(code)->Run()
			);
			Nan::Utf8String stackStr(stack);
			std::cout << *stackStr << std::endl;
			
		}
		
	}
	
	
	static inline void _wrapListener(
		const Nan::FunctionCallbackInfo<v8::Value> &info,
		bool isFront = false
	) {
		
		REQ_UTF8_ARG(0, name);
		REQ_FUN_ARG(1, cb);
		
		Nan::Persistent<v8::Function> persistentCb;
		persistentCb.Reset(cb);
		
		_addListener(info, *name, persistentCb, isFront);
		
	}
	
	
	static inline void _addOnceListener(
		const Nan::FunctionCallbackInfo<v8::Value> &info,
		const std::string &name,
		Nan::Persistent<v8::Function> &raw,
		Nan::Persistent<v8::Function> &cb,
		bool isFront
	) { THIS_EVENT_EMITTER;
		
		v8::Local<v8::Value> args[] = { info[0], info[1] };
		eventEmitter->emit("newListener", 2, args);
		
		if (isFront) {
			eventEmitter->_listeners[name].push_front(cb);
			eventEmitter->_raw[name].push_front(raw);
		} else {
			eventEmitter->_listeners[name].push_back(cb);
			eventEmitter->_raw[name].push_back(raw);
		}
		
		int nextId = eventEmitter->_freeId++;
		eventEmitter->_wrappedIds[nextId] = cb;
		eventEmitter->_rawIds[nextId] = raw;
		
		int count = eventEmitter->_raw[name].size();
		
		if (eventEmitter->_maxListeners > 0 && count > eventEmitter->_maxListeners) {
			
			std::cout << "EventEmitter Warning: too many listeners (";
			std::cout << count << " > " << eventEmitter->_maxListeners << ") on '";
			std::cout << name << "' event, possible memory leak." << std::endl;
			
			// Some JS magic to retrieve the call stack
			v8::Local<v8::String> code = JS_STR(
				"(new Error()).stack.split('\\n').slice(1).join('\\n')"
			);
			v8::Local<v8::String> stack = v8::Local<v8::String>::Cast(
				v8::Script::Compile(code)->Run()
			);
			Nan::Utf8String stackStr(stack);
			std::cout << *stackStr << std::endl;
			
		}
		
	}
	
	
	static inline void _wrapOnceListener(
		const Nan::FunctionCallbackInfo<v8::Value> &info,
		bool isFront = false
	) {
		
		REQ_UTF8_ARG(0, name);
		REQ_FUN_ARG(1, raw);
		
		v8::Local<v8::String> code = JS_STR(
			"((emitter, name, cb) => (...args) => {\n\
				cb(...args);\n\
				emitter.removeListener(name, cb);\n\
			})"
		);
		
		v8::Local<v8::Function> decor = v8::Local<v8::Function>::Cast(v8::Script::Compile(code)->Run());
		Nan::Callback decorCb(decor);
		v8::Local<v8::Value> argv[] = { info.This(), info[0], raw };
		v8::Local<v8::Function> wrap = v8::Local<v8::Function>::Cast(decorCb.Call(3, argv));
		
		Nan::Persistent<v8::Function> persistentWrap;
		persistentWrap.Reset(wrap);
		
		Nan::Persistent<v8::Function> persistentRaw;
		persistentRaw.Reset(raw);
		
		_addOnceListener(info, *name, persistentRaw, persistentWrap, isFront);
		
	}
	
	
	static NAN_METHOD(jsOn) { _wrapListener(info); }
	
	static NAN_METHOD(jsOnce) { _wrapOnceListener(info); }
	
	static NAN_METHOD(jsPrependListener) { _wrapListener(info, true); }
	
	static NAN_METHOD(jsPrependOnceListener) { _wrapOnceListener(info, true); }
	
	
	static NAN_METHOD(jsRemoveAllListeners) { THIS_EVENT_EMITTER;
		
		if (info.Length() > 0 && info[0]->IsString()) {
			
			MAP_TYPE tmpMap = eventEmitter->_raw;
			
			eventEmitter->_listeners.clear();
			eventEmitter->_raw.clear();
			eventEmitter->_wrappedIds.clear();
			eventEmitter->_rawIds.clear();
			
			for (MAP_IT_TYPE itMap = tmpMap.begin(); itMap != tmpMap.end(); ++itMap) {
				
				const std::string &current = itMap->first;
				VEC_TYPE &list = itMap->second;
				
				for (IT_TYPE it = list.begin(); it != list.end(); ++it) {
					
					v8::Local<v8::Value> args[] = { JS_STR(current.c_str()), Nan::New(*it) };
					eventEmitter->emit("removeListener", 2, args);
					
				}
				
			}
			
			return;
			
		}
		
		REQ_UTF8_ARG(0, n);
		
		std::string name = std::string(*n);
		VEC_TYPE &list = eventEmitter->_raw[name];
		
		if (list.empty()) {
			return;
		}
		
		if (eventEmitter->_rawIds.size()) {
			
			std::vector<int> removes;
			
			for (IT_TYPE it = list.begin(); it != list.end(); ++it) {
				
				FN_TYPE fn = *it;
				
				for (FNMAP_IT_TYPE itRaw = eventEmitter->_rawIds.begin(); itRaw != eventEmitter->_rawIds.end(); ++itRaw) {
					if (fn == itRaw->second) {
						removes.push_back(itRaw->first);
					}
				}
				
			}
			
			if (removes.size()) {
				for (std::vector<int>::const_iterator it = removes.begin(); it != removes.end(); ++it) {
					
					eventEmitter->_wrappedIds.erase(*it);
					eventEmitter->_rawIds.erase(*it);
					
				}
			}
			
		}
		
		
		VEC_TYPE tmpVec = eventEmitter->_raw[name];
		
		eventEmitter->_listeners[name].clear();
		eventEmitter->_raw[name].clear();
		
		for (IT_TYPE it = tmpVec.begin(); it != tmpVec.end(); ++it) {
			
			v8::Local<v8::Value> args[] = { JS_STR(name.c_str()), Nan::New(*it) };
			eventEmitter->emit("removeListener", 2, args);
			
		}
		
	}
	
	
	static NAN_METHOD(jsRemoveListener) { THIS_EVENT_EMITTER;
		
		REQ_UTF8_ARG(0, n);
		REQ_FUN_ARG(1, raw);
		
		Nan::Persistent<v8::Function> persistentRaw;
		persistentRaw.Reset(raw);
		
		std::string name = std::string(*n);
		
		VEC_TYPE &rawList = eventEmitter->_raw[name];
		
		if (rawList.empty()) {
			return;
		}
		
		v8::Local<v8::Value> args[] = { info[0], info[1] };
		
		for (IT_TYPE it = rawList.begin(); it != rawList.end(); ++it) {
			
			if (*it == persistentRaw) {
				rawList.erase(it);
				if (rawList.empty()) {
					eventEmitter->_raw.erase(name);
				}
				break;
			}
			
		}
		
		
		VEC_TYPE &wrapList = eventEmitter->_listeners[name];
		
		if (eventEmitter->_wrappedIds.size() == 0) {
			
			for (IT_TYPE it = wrapList.begin(); it != wrapList.end(); ++it) {
				
				if (*it == persistentRaw) {
					wrapList.erase(it);
					if (wrapList.empty()) {
						eventEmitter->_listeners.erase(name);
					}
					break;
				}
				
			}
			
			eventEmitter->emit("removeListener", 2, args);
			return;
			
		}
		
		
		for (FNMAP_IT_TYPE itRaw = eventEmitter->_rawIds.begin(); itRaw != eventEmitter->_rawIds.end(); ++itRaw) {
			
			if (persistentRaw == itRaw->second) {
				
				FN_TYPE fn = eventEmitter->_wrappedIds[itRaw->first];
				
				for (IT_TYPE it = wrapList.begin(); it != wrapList.end(); ++it) {
					
					if (*it == fn) {
						wrapList.erase(it);
						if (wrapList.empty()) {
							eventEmitter->_listeners.erase(name);
						}
						break;
					}
					
				}
				
				eventEmitter->_wrappedIds.erase(itRaw->first);
				eventEmitter->_rawIds.erase(itRaw->first);
				
				break;
				
			}
			
		}
		
		eventEmitter->emit("removeListener", 2, args);
		
	}
	
	
	static NAN_METHOD(jsSetMaxListeners) { THIS_EVENT_EMITTER;
		
		REQ_INT32_ARG(0, value);
		
		eventEmitter->_maxListeners = value;
		
	}
	
	
	static NAN_METHOD(jsRawListeners) { THIS_EVENT_EMITTER;
		
		REQ_UTF8_ARG(0, name);
		
		VEC_TYPE &list = eventEmitter->_raw[*name];
		
		v8::Local<v8::Array> jsListeners = Nan::New<v8::Array>(list.size());
		
		if (list.empty()) {
			RET_VALUE(jsListeners);
			return;
		}
		
		int i = 0;
		for (IT_TYPE it = list.begin(); it != list.end(); ++it, i++) {
			
			jsListeners->Set(JS_INT(i), Nan::New(*it));
			
		}
		
		RET_VALUE(jsListeners);
		
	}
	
	
private:
	
	bool _isDestroyed;
	
	int _maxListeners;
	
	MAP_TYPE _listeners;
	MAP_TYPE _raw;
	
	int _freeId;
	FNMAP_TYPE _wrappedIds;
	FNMAP_TYPE _rawIds;
	
};


#endif // _EVENT_EMITTER_
