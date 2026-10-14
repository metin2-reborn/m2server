/**
 *
 * @file	TrafficProfiler.h
 * @brief	TrafficProfiler class definition file
 * @author	Bang2ni
 * @version	05/07/07 Bang2ni - First release.
 * 
 */

#ifndef _METIN_II_TRAFFICPROFILER_H_
#define _METIN_II_TRAFFICPROFILER_H_

/**
 * @class	TrafficProfiler
 * @brief	Profiler that measures network I/O traffic per packet.
 * @author	Bang2ni
 * @version	05/07/07 Bang2ni - First release.
 *
 * Measures network I/O traffic per packet over time periods and writes a report as a text file.
 */
#include <common/singleton.h>

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <utility>
#include <vector>

class TrafficProfiler : public singleton< TrafficProfiler >
{
	public:

		/// I/O direction
		enum IODirection {
			IODIR_INPUT	= 0,	///< Input
			IODIR_OUTPUT,	///< Output
			IODIR_MAX
		};

	public:

		/// Constructor
		TrafficProfiler( void );

		/// Destructor
		~TrafficProfiler( void ) override;

		/// Initializes what profiling needs.
		/**
		 * @param [in]	dwFlushCycle flush cycle, in seconds.
		 * @param [in]	pszLogFileName name of the profiling log file
		 * @return	false if the profiling log file could not be opened.
		 *
		 * Opens (creates) the profiling log file.
		 */
		bool	Initialize( uint32_t dwFlushCycle, const char* pszLogFileName );

		/// Reports a packet that was or will be sent, for profiling.
		/**
		 * @param [in]	dir direction of the packet to profile
		 * @param [in]	byHeader packet header
		 * @param [in]	dwSize total size of the packet
		 * @return	Returns false if not initialized.
		 *
		 * Accumulates the size for the packet.
		 * If called after a flush cycle has elapsed since Initialize or the last Flush, flushes after reporting.
		 */
		bool	Report( IODirection dir, uint8_t byHeader, uint32_t dwSize )
		{
			ComputeTraffic( dir, byHeader, dwSize );
			if ( (uint32_t)(time( nullptr ) - m_tmProfileStartTime) >= m_dwFlushCycle )
				return Flush();
			return true;
		}

		/// Writes what has been reported so far to the file.
		/**
		 * @return	Not initialized.
		 */
		bool	Flush( void );

	private:

		/// Initializes profiling-related variables.
		void	InitializeProfiling( void );

		/// Computes the traffic of reported packets.
		/**
		 * @param [in]	dir direction of the packet to profile
		 * @param [in]	byHeader packet header
		 * @param [in]	dwSize total size of the packet
		 */
		void	ComputeTraffic( IODirection dir, uint8_t byHeader, uint32_t dwSize )
		{

			TrafficInfo& rTrafficInfo = m_aTrafficVec[ dir ][ byHeader ];

			m_dwTotalTraffic += dwSize;
			m_dwTotalPacket += !rTrafficInfo.second;

			rTrafficInfo.first += dwSize;
			rTrafficInfo.second++;
		}

		/// Traffic info type.
		/**
		 * first: accumulated total size
		 * second: number of times this packet was sent
		 */
		typedef std::pair< uint32_t, uint32_t >	TrafficInfo;

		/// Traffic info vector.
		typedef std::vector< TrafficInfo >	TrafficVec;

		FILE*		m_pfProfileLogFile;	///< Profile log file pointer
		uint32_t		m_dwFlushCycle;		///< Flush cycle
		time_t		m_tmProfileStartTime;	///< Time profiling started. Updated on every flush.
		uint32_t		m_dwTotalTraffic;	///< Total reported traffic volume
		uint32_t		m_dwTotalPacket;	///< Total number of reported packets
		TrafficVec	m_aTrafficVec[ IODIR_MAX ];	///< Array of vectors storing reported traffic. One vector per direction.
};

#endif // _METIN_II_TRAFFICPROFILER_H_
