#pragma once

// System Includes

//==================================================================================================
// CState

// CState Class
#include <cassert>

namespace metin2::game {

class CState
{
	public:
		// Destructor
		virtual ~CState()
		{
		}

		// State Functions
		virtual void ExecuteBeginState() = 0;
		virtual void ExecuteState() = 0;
		virtual void ExecuteEndState() = 0;
};

//==================================================================================================
// CStateTemplate

// CStateTemplate Class
template <class T>
class CStateTemplate : public CState
{
	public:
		typedef void (T::*PFNSTATE) (void);

	protected:
		T *             m_pInstance;            // Instance Pointer
		PFNSTATE        m_pfnBeginState;        // State Function Pointer
		PFNSTATE        m_pfnState;             // State Function Pointer
		PFNSTATE        m_pfnEndState;          // State Function Pointer

	public:
		// Constructor
		CStateTemplate() : m_pInstance(nullptr), m_pfnBeginState(nullptr), m_pfnState(nullptr), m_pfnEndState(nullptr)
		{
		}

		// Initialize Functions
		void Set(T *pInstance, PFNSTATE pfnBeginState, PFNSTATE pfnState, PFNSTATE pfnEndState)
		{
			// Set Instance
			assert(pInstance);
			m_pInstance = pInstance;

			// Set Function Pointers
			assert(pfnBeginState);
			m_pfnBeginState = pfnBeginState;

			assert(pfnState);
			m_pfnState = pfnState;

			assert(pfnEndState);
			m_pfnEndState = pfnEndState;
		}

		// State Functions
		void ExecuteBeginState() override
		{
			// Begin State
			assert(m_pInstance && m_pfnBeginState);
			(m_pInstance->*m_pfnBeginState)();
		}

		void ExecuteState() override
		{
			// State
			assert(m_pInstance && m_pfnState);
			(m_pInstance->*m_pfnState)();
		}

		void ExecuteEndState() override
		{
			// End State
			assert(m_pInstance && m_pfnEndState);
			(m_pInstance->*m_pfnEndState)();
		}
};
}
