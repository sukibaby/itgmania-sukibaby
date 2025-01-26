#include "global.h"
#include "LuaManager.h"
#include "LuaReference.h"
#include "RageUtil.h"
#include "RageLog.h"
#include "RageFile.h"
#include "RageThreads.h"
#include "arch/Dialog/Dialog.h"
#include "XmlFile.h"
#include "Command.h"
#include "RageLog.h"
#include "RageTypes.h"
#include "MessageManager.h"
#include "ver.h"

#include <cassert>
#include <cmath>
#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <map>
#include <sstream> // conversion for lua functions.
#include <vector>

LuaManager *LUA = nullptr;
struct Impl
{
	Impl(): g_pLock("Lua") {}
	std::vector<lua_State *> g_FreeStateList;
	std::map<lua_State *, bool> g_ActiveStates;

	RageMutex g_pLock;
};
static Impl *pImpl = nullptr;

#if defined(_MSC_VER)
	/* "interaction between '_setjmp' and C++ object destruction is non-portable"
	 * We don't care; we'll throw a fatal exception immediately anyway. */
	#pragma warning (disable : 4611)
#endif

/** @brief Utilities for working with Lua. */
namespace LuaHelpers
{
	template<> void Push<bool>( lua_State *L, const bool &Object );
	template<> void Push<float>( lua_State *L, const float &Object );
	template<> void Push<int>( lua_State *L, const int &Object );
	template<> void Push<RString>( lua_State *L, const RString &Object );

	template<> bool FromStack<bool>( Lua *L, bool &Object, int iOffset );
	template<> bool FromStack<float>( Lua *L, float &Object, int iOffset );
	template<> bool FromStack<int>( Lua *L, int &Object, int iOffset );
	template<> bool FromStack<unsigned int>( Lua *L, unsigned int &Object, int iOffset );
	template<> bool FromStack<RString>( Lua *L, RString &Object, int iOffset );

	bool InReportScriptError= false;
}

void LuaManager::SetGlobal( const RString &sName, int val )
{
	Lua *L = Get();
	LuaHelpers::Push( L, val );
	lua_setglobal( L, sName );
	Release( L );
}

void LuaManager::SetGlobal( const RString &sName, const RString &val )
{
	Lua *L = Get();
	LuaHelpers::Push( L, val );
	lua_setglobal( L, sName );
	Release( L );
}

void LuaManager::UnsetGlobal( const RString &sName )
{
	Lua *L = Get();
	lua_pushnil( L );
	lua_setglobal( L, sName );
	Release( L );
}

/** @brief Utilities for working with Lua. */
namespace LuaHelpers
{
	template<> void Push<bool>( lua_State *L, const bool &Object ) { lua_pushboolean( L, Object ); }
	template<> void Push<float>( lua_State *L, const float &Object ) { lua_pushnumber( L, Object ); }
	template<> void Push<double>( lua_State *L, const double &Object ) { lua_pushnumber( L, Object ); }
	template<> void Push<int>( lua_State *L, const int &Object ) { lua_pushinteger( L, Object ); }
	template<> void Push<unsigned int>( lua_State *L, const unsigned int &Object ) { lua_pushnumber( L, double(Object) ); }
	template<> void Push<unsigned long long>( lua_State *L, const unsigned long long &Object ) { lua_pushnumber( L, double(Object) ); }
	template<> void Push<RString>( lua_State *L, const RString &Object ) { lua_pushlstring( L, Object.data(), Object.size() ); }
	template<> void Push<std::string>( lua_State *L, std::string const& object ) { lua_pushlstring( L, object.data(), object.size() ); }

	template<> bool FromStack<bool>( Lua *L, bool &Object, int iOffset ) { Object = !!lua_toboolean( L, iOffset ); return true; }
	template<> bool FromStack<float>( Lua *L, float &Object, int iOffset ) { Object = (float)lua_tonumber( L, iOffset ); return true; }
	template<> bool FromStack<int>( Lua *L, int &Object, int iOffset ) { Object = lua_tointeger( L, iOffset ); return true; }
	template<> bool FromStack<unsigned int>( Lua *L, unsigned int &Object, int iOffset ) { Object = lua_tointeger( L, iOffset ); return true; }
	template<> bool FromStack<RString>( Lua *L, RString &Object, int iOffset )
	{
		size_t iLen;
		const char *pStr = lua_tolstring( L, iOffset, &iLen );
		if( pStr != nullptr )
			Object.assign( pStr, iLen );
		else
			Object.clear();

		return pStr != nullptr;
	}
}

void LuaHelpers::CreateTableFromArrayB( Lua *L, const std::vector<bool> &aIn )
{
	lua_newtable( L );
	for( unsigned i = 0; i < aIn.size(); ++i )
	{
		lua_pushboolean( L, aIn[i] );
		lua_rawseti( L, -2, i+1 );
	}
}

void LuaHelpers::ReadArrayFromTableB( Lua *L, std::vector<bool> &aOut )
{
	luaL_checktype( L, -1, LUA_TTABLE );

	for( unsigned i = 0; i < aOut.size(); ++i )
	{
		lua_rawgeti( L, -1, i+1 );
		bool bOn = !!lua_toboolean( L, -1 );
		aOut[i] = bOn;
		lua_pop( L, 1 );
	}
}

namespace
{
	// Creates a table from an XNode and leaves it on the stack.
	void CreateTableFromXNodeRecursive( Lua *L, const XNode *pNode )
	{
		// create our base table
		lua_newtable( L );


		FOREACH_CONST_Attr( pNode, pAttr )
		{
			lua_pushstring( L, pAttr->first );			// push key
			pNode->PushAttrValue( L, pAttr->first );	// push value

			//add key-value pair to our table
			lua_settable( L, -3 );
		}

		FOREACH_CONST_Child( pNode, c )
		{
			const XNode *pChild = c;
			lua_pushstring( L, pChild->m_sName ); // push key

			// push value (more correctly, build this child's table and leave it there)
			CreateTableFromXNodeRecursive( L, pChild );

			// add key-value pair to the table
			lua_settable( L, -3 );
		}
	}
}

void LuaHelpers::CreateTableFromXNode( Lua *L, const XNode *pNode )
{
	// This creates our table and leaves it on the stack.
	CreateTableFromXNodeRecursive( L, pNode );
}

static int GetLuaStack( lua_State *L )
{
	RString sErr;
	LuaHelpers::Pop( L, sErr );

	lua_Debug ar;

	for( int iLevel = 0; lua_getstack(L, iLevel, &ar); ++iLevel )
	{
		if( !lua_getinfo(L, "nSluf", &ar) )
			break;
		// The function is now on the top of the stack.
		const char *file = ar.source[0] == '@' ? ar.source + 1 : ar.short_src;
		const char *name;
		std::vector<RString> vArgs;

		if( !strcmp(ar.what, "C") )
		{
			for( int i = 1; i <= ar.nups && (name = lua_getupvalue(L, -1, i)) != nullptr; ++i )
			{
				vArgs.push_back( ssprintf("%s = %s", name, lua_tostring(L, -1)) );
				lua_pop( L, 1 ); // pop value
			}
		}
		else
		{
			for( int i = 1; (name = lua_getlocal(L, &ar, i)) != nullptr; ++i )
			{
				vArgs.push_back( ssprintf("%s = %s", name, lua_tostring(L, -1)) );
				lua_pop( L, 1 ); // pop value
			}
		}

		// If the first call is this function, omit it from the trace.
		if( iLevel == 0 && lua_iscfunction(L, -1) && lua_tocfunction(L, 1) == GetLuaStack )
		{
			lua_pop( L, 1 ); // pop function
			continue;
		}
		lua_pop( L, 1 ); // pop function

		sErr += ssprintf( "\n%s:", file );
		if( ar.currentline != -1 )
			sErr += ssprintf( "%i:", ar.currentline );

		if( ar.name && ar.name[0] )
			sErr += ssprintf( " %s", ar.name );
		else if( !strcmp(ar.what, "main") || !strcmp(ar.what, "tail") || !strcmp(ar.what, "C") )
			sErr += ssprintf( " %s", ar.what );
		else
			sErr += ssprintf( " unknown" );
		sErr += ssprintf( "(%s)", join(",", vArgs).c_str() );
	}

	LuaHelpers::Push( L, sErr );
	return 1;
}


static int LuaPanic( lua_State *L )
{
	GetLuaStack( L );

	RString sErr;
	LuaHelpers::Pop( L, sErr );

	RageException::Throw( "[Lua panic] %s", sErr.c_str() );
}

// Actor registration
static std::vector<RegisterWithLuaFn>	*g_vRegisterActorTypes = nullptr;

void LuaManager::Register( RegisterWithLuaFn pfn )
{
	if( g_vRegisterActorTypes == nullptr )
		g_vRegisterActorTypes = new std::vector<RegisterWithLuaFn>;

	g_vRegisterActorTypes->push_back( pfn );
}


LuaManager::LuaManager()
{
	pImpl = new Impl;
	LUA = this; // so that LUA is available when we call the Register functions

	lua_State *L = lua_open();
	ASSERT( L != nullptr );

	lua_atpanic( L, LuaPanic );
	m_pLuaMain = L;

	lua_pushcfunction( L, luaopen_base ); lua_call( L, 0, 0 );
	lua_pushcfunction( L, luaopen_math ); lua_call( L, 0, 0 );
	lua_pushcfunction( L, luaopen_string ); lua_call( L, 0, 0 );
	lua_pushcfunction( L, luaopen_table ); lua_call( L, 0, 0 );
	lua_pushcfunction( L, luaopen_debug ); lua_call( L, 0, 0 );

	// Store the thread pool in a table on the stack, in the main thread.
#define THREAD_POOL 1
	lua_newtable( L );

	RegisterTypes();
}

LuaManager::~LuaManager()
{
	lua_close( m_pLuaMain );
	RageUtil::SafeDelete( pImpl );
}

Lua *LuaManager::Get()
{
	bool bLocked = false;
	if( !pImpl->g_pLock.IsLockedByThisThread() )
	{
		pImpl->g_pLock.Lock();
		bLocked = true;
	}

	ASSERT( lua_gettop(m_pLuaMain) == 1 );

	lua_State *pRet;
	if( pImpl->g_FreeStateList.empty() )
	{
		pRet = lua_newthread( m_pLuaMain );

		// Store the new thread in THREAD_POOL, so it isn't collected.
		int iLast = lua_objlen( m_pLuaMain, THREAD_POOL );
		lua_rawseti( m_pLuaMain, THREAD_POOL, iLast+1 );
	}
	else
	{
		pRet = pImpl->g_FreeStateList.back();
		pImpl->g_FreeStateList.pop_back();
	}

	pImpl->g_ActiveStates[pRet] = bLocked;
	return pRet;
}

void LuaManager::Release( Lua *&p )
{
	pImpl->g_FreeStateList.push_back( p );

	ASSERT( lua_gettop(p) == 0 );
	ASSERT( pImpl->g_ActiveStates.find(p) != pImpl->g_ActiveStates.end() );
	bool bDoUnlock = pImpl->g_ActiveStates[p];
	pImpl->g_ActiveStates.erase( p );

	if( bDoUnlock )
		pImpl->g_pLock.Unlock();
	p = nullptr;
}

/*
 * Low-level access to Lua is always serialized through pImpl->g_pLock; we never run the Lua
 * core simultaneously from multiple threads.  However, when a thread has an acquired
 * lua_State, it can release Lua for use by other threads.  This allows Lua bindings
 * to process long-running actions, without blocking all other threads from using Lua
 * until it finishes.
 *
 * Lua *L = LUA->Get();			// acquires L and locks Lua
 * lua_newtable(L);				// does something with Lua
 * LUA->YieldLua();				// unlocks Lua for lengthy operation; L is still owned, but can't be used
 * RString s = ReadFile("/filename.txt");	// time-consuming operation; other threads may use Lua in the meantime
 * LUA->UnyieldLua();			// relock Lua
 * lua_pushstring( L, s );		// finish working with it
 * LUA->Release( L );			// release L and unlock Lua
 *
 * YieldLua() must not be called when already yielded, or when a lua_State has not been
 * acquired (you have nothing to yield), and always unyield before releasing the
 * state.  Recursive handling is OK:
 *
 * L1 = LUA->Get();
 * LUA->YieldLua();				// yields
 *   L2 = LUA->Get();			// unyields
 *   LUA->Release(L2);			// re-yields
 * LUA->UnyieldLua();
 * LUA->Release(L1);
 */
void LuaManager::YieldLua()
{
	ASSERT( pImpl->g_pLock.IsLockedByThisThread() );

	pImpl->g_pLock.Unlock();
}

void LuaManager::UnyieldLua()
{
	pImpl->g_pLock.Lock();
}

void LuaManager::RegisterTypes()
{
	Lua *L = Get();

	if( g_vRegisterActorTypes )
	{
		for( unsigned i=0; i<g_vRegisterActorTypes->size(); i++ )
		{
			RegisterWithLuaFn fn = (*g_vRegisterActorTypes)[i];
			fn( L );
		}
	}

	Release( L );
}

LuaThreadVariable::LuaThreadVariable( const RString &sName, const RString &sValue )
{
	m_Name = new LuaReference;
	m_pOldValue = new LuaReference;

	Lua *L = LUA->Get();
	LuaHelpers::Push( L, sName );
	m_Name->SetFromStack( L );
	LuaHelpers::Push( L, sValue );
	SetFromStack( L );
	LUA->Release( L );
}

LuaThreadVariable::LuaThreadVariable( const RString &sName, const LuaReference &Value )
{
	m_Name = new LuaReference;
	m_pOldValue = new LuaReference;

	Lua *L = LUA->Get();
	LuaHelpers::Push( L, sName );
	m_Name->SetFromStack( L );

	Value.PushSelf( L );
	SetFromStack( L );
	LUA->Release( L );
}

// name and value are on the stack
LuaThreadVariable::LuaThreadVariable( lua_State *L )
{
	m_Name = new LuaReference;
	m_pOldValue = new LuaReference;

	lua_pushvalue( L, -2 );
	m_Name->SetFromStack( L );

	SetFromStack( L );

	lua_pop( L, 1 );
}

RString LuaThreadVariable::GetCurrentThreadIDString()
{
	uint64_t iID = RageThread::GetCurrentThreadID();
	return ssprintf( "%08x%08x", uint32_t(iID >> 32), uint32_t(iID) );
}

bool LuaThreadVariable::PushThreadTable( lua_State *L, bool bCreate )
{
	lua_getfield( L, LUA_REGISTRYINDEX, "LuaThreadVariableTable" );
	if( lua_isnil(L, -1) )
	{
		lua_pop( L, 1 );
		if( !bCreate )
			return false;
		lua_newtable( L );

		lua_pushvalue( L, -1 );
		lua_setfield( L, LUA_REGISTRYINDEX, "LuaThreadVariableTable" );
	}

	RString sThreadIDString = GetCurrentThreadIDString();
	LuaHelpers::Push( L, sThreadIDString );
	lua_gettable( L, -2 );
	if( lua_isnil(L, -1) )
	{
		lua_pop( L, 1 );
		if( !bCreate )
		{
			lua_pop( L, 1 );
			return false;
		}
		lua_newtable( L );

		lua_pushinteger( L, 0 );
		lua_rawseti( L, -2, 0 );

		LuaHelpers::Push( L, sThreadIDString );
		lua_pushvalue( L, -2 );
		lua_settable( L, -4 );
	}

	lua_remove( L, -2 );
	return true;
}

void LuaThreadVariable::GetThreadVariable( lua_State *L )
{
	if( !PushThreadTable(L, false) )
	{
		lua_pop( L, 1 );
		lua_pushnil( L );
		return;
	}

	lua_pushvalue( L, -2 );
	lua_gettable( L, -2 );
	lua_remove( L, -2 );
	lua_remove( L, -2 );
}

int LuaThreadVariable::AdjustCount( lua_State *L, int iAdd )
{
	ASSERT( lua_istable(L, -1) );

	lua_rawgeti( L, -1, 0 );
	ASSERT( lua_isnumber(L, -1) != 0 );

	int iCount = lua_tointeger( L, -1 );
	lua_pop( L, 1 );

	iCount += iAdd;
	lua_pushinteger( L, iCount );
	lua_rawseti( L, -2, 0 );

	return iCount;
}

void LuaThreadVariable::SetFromStack( lua_State *L )
{
	ASSERT( !m_pOldValue->IsSet() ); // don't call twice

	PushThreadTable( L, true );

	m_Name->PushSelf( L );
	lua_gettable( L, -2 );
	m_pOldValue->SetFromStack( L );

	m_Name->PushSelf( L );
	lua_pushvalue( L, -3 );
	lua_settable( L, -3 );

	AdjustCount( L, +1 );

	lua_pop( L, 2 );
}

LuaThreadVariable::~LuaThreadVariable()
{
	Lua *L = LUA->Get();

	PushThreadTable( L, true );
	m_Name->PushSelf( L );
	m_pOldValue->PushSelf( L );
	lua_settable( L, -3 );

	if( AdjustCount( L, -1 ) == 0 )
	{
		// if empty, delete the table
		lua_getfield( L, LUA_REGISTRYINDEX, "LuaThreadVariableTable" );
		ASSERT( lua_istable(L, -1) );

		LuaHelpers::Push( L, GetCurrentThreadIDString() );
		lua_pushnil( L );
		lua_settable( L, -3 );
		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );

	LUA->Release( L );

	delete m_pOldValue;
	delete m_Name;
}

namespace
{
	struct LClass
	{
		RString m_sBaseName;
		std::vector<RString> m_vMethods;
	};
}

XNode *LuaHelpers::GetLuaInformation()
{
	XNode *pLuaNode = new XNode( "Lua" );

	XNode *pGlobalsNode = pLuaNode->AppendChild( "GlobalFunctions" );
	XNode *pClassesNode = pLuaNode->AppendChild( "Classes" );
	XNode *pNamespacesNode = pLuaNode->AppendChild( "Namespaces" );
	XNode *pSingletonsNode = pLuaNode->AppendChild( "Singletons" );
	XNode *pEnumsNode = pLuaNode->AppendChild( "Enums" );
	XNode *pConstantsNode = pLuaNode->AppendChild( "Constants" );

	std::vector<RString> vFunctions;
	std::map<RString, LClass> mClasses;
	std::map<RString, std::vector<RString>> mNamespaces;
	std::map<RString, RString> mSingletons;
	std::map<RString, float> mConstants;
	std::map<RString, RString> mStringConstants;
	std::map<RString, std::vector<RString>> mEnums;

	Lua *L = LUA->Get();
	FOREACH_LUATABLE( L, LUA_GLOBALSINDEX )
	{
		RString sKey;
		LuaHelpers::Pop( L, sKey );

		switch( lua_type(L, -1) )
		{
		case LUA_TTABLE:
		{
			if( luaL_getmetafield(L, -1, "class") )
			{
				const char *name = lua_tostring( L, -1 );

				if( !name )
					break;
				LClass &c = mClasses[name];
				lua_pop( L, 1 ); // pop name

				// Get base class.
				luaL_getmetatable( L, name );
				ASSERT( !lua_isnil(L, -1) );
				lua_getfield( L, -1, "base" );
				name = lua_tostring( L, -1 );

				if( name )
					c.m_sBaseName = name;
				lua_pop( L, 2 ); // pop name and metatable

				// Get methods.
				FOREACH_LUATABLE( L, -1 )
				{
					RString sMethod;
					if( LuaHelpers::FromStack(L, sMethod, -1) )
						c.m_vMethods.push_back( sMethod );
				}
				sort( c.m_vMethods.begin(), c.m_vMethods.end() );
				break;
			}
			[[fallthrough]];
		}
		case LUA_TUSERDATA: // table or userdata: class instance
		{
			if( !luaL_callmeta(L, -1, "__type") )
				break;
			RString sType;
			if( !LuaHelpers::Pop(L, sType) )
				break;
			if( sType == "Enum" )
				LuaHelpers::ReadArrayFromTable( mEnums[sKey], L );
			else
				mSingletons[sKey] = sType;
			break;
		}
		case LUA_TNUMBER:
			LuaHelpers::FromStack( L, mConstants[sKey], -1 );
			break;
		case LUA_TSTRING:
			LuaHelpers::FromStack( L, mStringConstants[sKey], -1 );
			break;
		case LUA_TFUNCTION:
			vFunctions.push_back( sKey );
			/*
			{
				lua_Debug ar;
				lua_getfield( L, LUA_GLOBALSINDEX, sKey );
				lua_getinfo( L, ">S", &ar ); // Pops the function
				printf( "%s: %s\n", sKey.c_str(), ar.short_src );
			}
			*/
			break;
		}
	}

	// Find namespaces
	lua_pushcfunction( L, luaopen_package ); lua_call( L, 0, 0 );
	lua_getglobal( L, "package" );
	ASSERT( lua_istable(L, -1) );
	lua_getfield( L, -1, "loaded" );
	ASSERT( lua_istable(L, -1) );

	//const RString BuiltInPackages[] = { "_G", "coroutine", "debug", "math", "package", "string", "table" };
	const RString BuiltInPackages[] = { "_G", "coroutine", "debug", "math", "package", "string", "table" };
	const RString *const end = BuiltInPackages+ARRAYLEN(BuiltInPackages);
	FOREACH_LUATABLE( L, -1 )
	{
		RString sNamespace;
		LuaHelpers::Pop( L, sNamespace );
		if( find(BuiltInPackages, end, sNamespace) != end )
			continue;
		std::vector<RString> &vNamespaceFunctions = mNamespaces[sNamespace];
		FOREACH_LUATABLE( L, -1 )
		{
			RString sFunction;
			LuaHelpers::Pop( L, sFunction );
			vNamespaceFunctions.push_back( sFunction );
		}
		sort( vNamespaceFunctions.begin(), vNamespaceFunctions.end() );
	}
	lua_pop( L, 2 );

	LUA->Release( L );

	/* Globals */
	sort( vFunctions.begin(), vFunctions.end() );
	for (RString const &func : vFunctions)
	{
		XNode *pFunctionNode = pGlobalsNode->AppendChild( "Function" );
		pFunctionNode->AppendAttr( "name", func );
	}

	/* Classes */
	for (auto const &c : mClasses)
	{
		XNode *pClassNode = pClassesNode->AppendChild( "Class" );

		pClassNode->AppendAttr( "name", c.first );
		if( !c.second.m_sBaseName.empty() )
			pClassNode->AppendAttr( "base", c.second.m_sBaseName );
		for (RString const & m : c.second.m_vMethods)
		{
			XNode *pMethodNode = pClassNode->AppendChild( "Function" );
			pMethodNode->AppendAttr( "name", m );
		}
	}

	/* Singletons */
	for (auto const &s : mSingletons)
	{
		if( mClasses.find(s.first) != mClasses.end() )
			continue;
		XNode *pSingletonNode = pSingletonsNode->AppendChild( "Singleton" );
		pSingletonNode->AppendAttr( "name", s.first );
		pSingletonNode->AppendAttr( "class", s.second );
	}

	/* Namespaces */
	for( std::map<RString, std::vector<RString>>::const_iterator iter = mNamespaces.begin(); iter != mNamespaces.end(); ++iter )
	{
		XNode *pNamespaceNode = pNamespacesNode->AppendChild( "Namespace" );
		const std::vector<RString> &vNamespace = iter->second;
		pNamespaceNode->AppendAttr( "name", iter->first );

		for (RString const &func: vNamespace)
		{
			XNode *pFunctionNode = pNamespaceNode->AppendChild( "Function" );
			pFunctionNode->AppendAttr( "name", func );
		}
	}

	/* Enums */
	for( std::map<RString, std::vector<RString>>::const_iterator iter = mEnums.begin(); iter != mEnums.end(); ++iter )
	{
		XNode *pEnumNode = pEnumsNode->AppendChild( "Enum" );

		const std::vector<RString> &vEnum = iter->second;
		pEnumNode->AppendAttr( "name", iter->first );

		for( unsigned i = 0; i < vEnum.size(); ++i )
		{
			XNode *pEnumValueNode = pEnumNode->AppendChild( "EnumValue" );
			pEnumValueNode->AppendAttr( "name", ssprintf("'%s'", vEnum[i].c_str()) );
			pEnumValueNode->AppendAttr( "value", i );
		}
	}

	/* Constants, String Constants */
	for (auto const &c : mConstants)
	{
		XNode *pConstantNode = pConstantsNode->AppendChild( "Constant" );

		pConstantNode->AppendAttr( "name", c.first );
		if( c.second == std::trunc(c.second) )
			pConstantNode->AppendAttr( "value", static_cast<int>(c.second) );
		else
			pConstantNode->AppendAttr( "value", c.second );
	}
	for (auto const &s : mStringConstants)
	{
		XNode *pConstantNode = pConstantsNode->AppendChild( "Constant" );
		pConstantNode->AppendAttr( "name", s.first );
		pConstantNode->AppendAttr( "value", ssprintf("'%s'", s.second.c_str()) );
	}

	return pLuaNode;
}

bool LuaHelpers::RunScriptFile( const RString &sFile )
{
	RString sScript;
	if( !GetFileContents(sFile, sScript) )
		return false;

	Lua *L = LUA->Get();

	RString sError;
	if( !LuaHelpers::RunScript( L, sScript, "@" + sFile, sError, 0 ) )
	{
		LUA->Release( L );
		sError = ssprintf( "Lua runtime error: %s", sError.c_str() );
		LuaHelpers::ReportScriptError(sError);
		return false;
	}
	LUA->Release( L );

	return true;
}


bool LuaHelpers::LoadScript( Lua *L, const RString &sScript, const RString &sName, RString &sError )
{
	// load string
	int ret = luaL_loadbuffer( L, sScript.data(), sScript.size(), sName );
	if( ret )
	{
		LuaHelpers::Pop( L, sError );
		return false;
	}

	return true;
}

void LuaHelpers::ScriptErrorMessage(RString const& Error)
{
	Message msg("ScriptError");
	msg.SetParam("message", Error);
	MESSAGEMAN->Broadcast(msg);
}

Dialog::Result LuaHelpers::ReportScriptError(RString const& Error, RString ErrorType, bool UseAbort)
{
	// Protect from a recursion loop resulting from a mistake in the error reporting lua.
	if(!InReportScriptError)
	{
		InReportScriptError= true;
		ScriptErrorMessage(Error);
		InReportScriptError= false;
	}
	LOG->Warn( "%s", Error.c_str());
	if(UseAbort)
	{
		RString with_correct= Error + "  Correct this and click Retry, or Cancel to break.";
		return Dialog::AbortRetryIgnore(with_correct, ErrorType);
	}
	//Dialog::OK(Error, ErrorType);
	return Dialog::ok;
}

// For convenience when replacing uses of LOG->Warn.
void LuaHelpers::ReportScriptErrorFmt(const char *fmt, ...)
{
	va_list	va;
	va_start( va, fmt );
	RString Buff = vssprintf( fmt, va );
	va_end( va );
	ReportScriptError(Buff);
}

bool LuaHelpers::RunScriptOnStack( Lua *L, RString &Error, int Args, int ReturnValues, bool ReportError )
{
	lua_pushcfunction( L, GetLuaStack );

	// move the error function above the function and params
	int ErrFunc = lua_gettop(L) - Args - 1;
	lua_insert( L, ErrFunc );

	// evaluate
	int ret = lua_pcall( L, Args, ReturnValues, ErrFunc );
	if( ret )
	{
		if(ReportError)
		{
			RString lerror;
			LuaHelpers::Pop( L, lerror );
			Error+= lerror;
			ReportScriptError(Error);
		}
		else
		{
			LuaHelpers::Pop( L, Error );
		}
		lua_remove( L, ErrFunc );
		for( int i = 0; i < ReturnValues; ++i )
			lua_pushnil( L );
		return false;
	}

	lua_remove( L, ErrFunc );
	return true;
}

bool LuaHelpers::RunScript( Lua *L, const RString &Script, const RString &Name, RString &Error, int Args, int ReturnValues, bool ReportError )
{
	RString lerror;
	if( !LoadScript(L, Script, Name, lerror) )
	{
		Error+= lerror;
		if(ReportError)
		{
			ReportScriptError(Error);
		}
		lua_pop( L, Args );
		for( int i = 0; i < ReturnValues; ++i )
			lua_pushnil( L );
		return false;
	}

	// move the function above the params
	lua_insert( L, lua_gettop(L) - Args );

	return LuaHelpers::RunScriptOnStack( L, Error, Args, ReturnValues, ReportError );
}

bool LuaHelpers::RunExpression( Lua *L, const RString &sExpression, const RString &sName )
{
	RString sError= ssprintf("Lua runtime error parsing \"%s\": ", sName.size()? sName.c_str():sExpression.c_str());
	if(!LuaHelpers::RunScript(L, "return " + sExpression, sName.empty()? RString("in"):sName, sError, 0, 1, true))
	{
		return false;
	}
	return true;
}

void LuaHelpers::ParseCommandList( Lua *L, const RString &sCommands, const RString &sName, bool bLegacy )
{
	RString sLuaFunction;
	if( sCommands.size() > 0 && sCommands[0] == '\033' )
	{
		// This is a compiled Lua chunk. Just pass it on directly.
		sLuaFunction = sCommands;
	}
	else if( sCommands.size() > 0 && sCommands[0] == '%' )
	{
		sLuaFunction = "return ";
		sLuaFunction.append( sCommands.begin()+1, sCommands.end() );
	}
	else
	{
		Commands cmds;
		ParseCommands( sCommands, cmds, bLegacy );

		// Convert cmds to a Lua function
		std::ostringstream s;

		s << "return function(self)\n";

		if( bLegacy )
			s << "\tparent = self:GetParent();\n";

		for (Command const &cmd : cmds.v)
		{
			RString sCmdName = cmd.GetName();
			if( bLegacy )
				sCmdName.MakeLower();
			s << "\tself:" << sCmdName << "(";

			bool bFirstParamIsString = bLegacy && (
					sCmdName == "horizalign" ||
					sCmdName == "vertalign" ||
					sCmdName == "effectclock" ||
					sCmdName == "blend" ||
					sCmdName == "ztestmode" ||
					sCmdName == "cullmode" ||
					sCmdName == "playcommand" ||
					sCmdName == "queuecommand" ||
					sCmdName == "queuemessage" ||
					sCmdName == "settext");

			for( unsigned i=1; i<cmd.m_vsArgs.size(); i++ )
			{
				RString sArg = cmd.m_vsArgs[i];

				// "+200" -> "200"
				if( sArg[0] == '+' )
					sArg.erase( sArg.begin() );

				if( i==1 && bFirstParamIsString ) // string literal, legacy only
				{
					sArg.Replace( "'", "\\'" );	// escape quote
					s << "'" << sArg << "'";
				}
				else if( sArg[0] == '#' )	// HTML color
				{
					RageColor col;	// in case FromString fails
					col.FromString( sArg );
					// col is still valid if FromString fails
					s << col.r << "," << col.g << "," << col.b << "," << col.a;
				}
				else
				{
					s << sArg;
				}

				if( i != cmd.m_vsArgs.size()-1 )
					s << ",";
			}
			s << ")\n";
		}

		s << "end\n";

		sLuaFunction = s.str();
	}

	RString sError;
	if( !LuaHelpers::RunScript(L, sLuaFunction, sName, sError, 0, 1) )
		LOG->Warn( "Compiling \"%s\": %s", sLuaFunction.c_str(), sError.c_str() );

	// The function is now on the stack.
}

/* Like luaL_typerror, but without the special case for argument 1 being "self"
 * in method calls, so we give a correct error message after we remove self. */
int LuaHelpers::TypeError( Lua *L, int iArgNo, const char *szName )
{
	RString sType;
	luaL_pushtype( L, iArgNo );
	LuaHelpers::Pop( L, sType );

	lua_Debug debug;
	if( !lua_getstack( L, 0, &debug ) )
	{
		return luaL_error( L, "invalid type (%s expected, got %s)",
			szName, sType.c_str() );
	}
	else
	{
		lua_getinfo( L, "n", &debug );
		return luaL_error( L, "bad argument #%d to \"%s\" (%s expected, got %s)",
			iArgNo, debug.name? debug.name:"(unknown)", szName, sType.c_str() );
	}
}

void LuaHelpers::DeepCopy( lua_State *L )
{
	luaL_checktype( L, -2, LUA_TTABLE );
	luaL_checktype( L, -1, LUA_TTABLE );

	// Call DeepCopy(t, u), where t is our referenced object and u is the new table.
	lua_getglobal( L, "DeepCopy" );

	ASSERT_M( !lua_isnil(L, -1), "DeepCopy() missing" );
	ASSERT_M( lua_isfunction(L, -1), "DeepCopy() not a function" );
	lua_insert( L, lua_gettop(L)-2 );

	lua_call( L, 2, 0 );
}

namespace
{
	int lua_pushvalues( lua_State *L )
	{
		int iArgs = lua_tointeger( L, lua_upvalueindex(1) );
		for( int i = 0; i < iArgs; ++i )
			lua_pushvalue( L, lua_upvalueindex(i+2) );
		return iArgs;
	}
}

void LuaHelpers::PushValueFunc( lua_State *L, int iArgs )
{
	int iTop = lua_gettop( L ) - iArgs + 1;
	lua_pushinteger( L, iArgs );
	lua_insert( L, iTop );
	lua_pushcclosure( L, lua_pushvalues, iArgs+1 );
}

#include "ProductInfo.h"
const static RString ITGmaniaName = "ITGmania";
const static RString ITGmaniaVersion = "0.9.0 [unoffocial build 1-26-2025 do not use]";
LuaFunction(ProductFamily, ITGmaniaName);
LuaFunction(ProductVersion, ITGmaniaVersion);
LuaFunction(ProductID, ITGmaniaName);

extern char const * const version_date;
extern char const * const version_time;
LuaFunction( VersionDate, (RString) version_date );
LuaFunction( VersionTime, (RString) version_time );

static float scale( float x, float l1, float h1, float l2, float h2 )
{
	return SCALE( x, l1, h1, l2, h2 );
}
LuaFunction( scale, scale(FArg(1), FArg(2), FArg(3), FArg(4), FArg(5)) );

LuaFunction( clamp, std::clamp(FArg(1), FArg(2), FArg(3)) );

#include "LuaBinding.h"
namespace
{
	static int Trace( lua_State *L )
	{
		RString sString = SArg(1);
		LOG->Trace( "%s", sString.c_str() );
		return 0;
	}
	static int Warn( lua_State *L )
	{
		RString sString = SArg(1);
		LOG->Warn( "%s", sString.c_str() );
		return 0;
	}
	static int Flush( lua_State *L )
	{
		LOG->Flush();
		return 0;
	}
	static int CheckType( lua_State *L )
	{
		RString sType = SArg(1);
		bool bRet = LuaBinding::CheckLuaObjectType( L, 2, sType );
		LuaHelpers::Push( L, bRet );
		return 1;
	}
	static int ReadFile( lua_State *L )
	{
		RString sPath = SArg(1);

		/* Release Lua while we call GetFileContents, so we don't access
		 * it while we read from the disk. */
		LUA->YieldLua();

		RString sFileContents;
		bool bRet = GetFileContents( sPath, sFileContents );

		LUA->UnyieldLua();
		if( !bRet )
		{
			lua_pushnil( L );
			lua_pushstring( L, "error" ); // XXX
			return 2;
		}
		else
		{
			LuaHelpers::Push( L, sFileContents );
			return 1;
		}
	}

	/* RunWithThreadVariables(func, { a = "x", b = "y" }, arg1, arg2, arg3 ... }
	 * calls func(arg1, arg2, arg3) with two LuaThreadVariable set, and returns
	 * the return values of func(). */
	static int RunWithThreadVariables( lua_State *L )
	{
		luaL_checktype( L, 1, LUA_TFUNCTION );
		luaL_checktype( L, 2, LUA_TTABLE );

		std::vector<LuaThreadVariable *> apVars;
		FOREACH_LUATABLE( L, 2 )
		{
			lua_pushvalue( L, -2 );
			LuaThreadVariable *pVar = new LuaThreadVariable( L );
			apVars.push_back( pVar );
		}

		lua_remove( L, 2 );

		/* XXX: We want to clean up apVars on errors, but if we lua_pcall,
		 * we won't propagate the error upwards. */
		int iArgs = lua_gettop(L) - 1;
		lua_call( L, iArgs, LUA_MULTRET );
		int iVals = lua_gettop(L);

		for (LuaThreadVariable *v : apVars)
			delete v;
		return iVals;
	}

	static int GetThreadVariable( lua_State *L )
	{
		luaL_checkstring( L, 1 );
		lua_pushvalue( L, 1 );
		LuaThreadVariable::GetThreadVariable( L );
		return 1;
	}

	static int ReportScriptError(lua_State* L)
	{
		RString error= "Script error occurred.";
		RString error_type= "LUA_ERROR";
		if(lua_isstring(L, 1))
		{
			error= SArg(1);
		}
		if(lua_isstring(L, 2))
		{
			error_type= SArg(2);
		}
		LuaHelpers::ReportScriptError(error, error_type);
		return 0;
	}

	const luaL_Reg luaTable[] =
	{
		LIST_METHOD( Trace ),
		LIST_METHOD( Warn ),
		LIST_METHOD( Flush ),
		LIST_METHOD( CheckType ),
		LIST_METHOD( ReadFile ),
		LIST_METHOD( RunWithThreadVariables ),
		LIST_METHOD( GetThreadVariable ),
		LIST_METHOD( ReportScriptError ),
		{ nullptr, nullptr }
	};
}

LUA_REGISTER_NAMESPACE( lua )

/*
 * (c) 2004-2006 Glenn Maynard, Steve Checkoway
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
