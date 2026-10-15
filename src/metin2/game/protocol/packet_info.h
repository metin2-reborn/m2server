#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace metin2::game::inline protocol {

struct packet_element {
	int		size;
	std::string	name;
	int		called;
	uint32_t	load;
	bool	sequence_packet;
};

class packet_info
{
	public:
		packet_info();
		virtual ~packet_info();

		void set(int header, int size_value, const char * name_value, bool seq=false);
		bool get(int header, int * size, const char ** ppsz_name);

		void start();
		void end();

		void log(const char * file_name);

		bool is_sequence(int header);
		void set_sequence(int header, bool seq);

	private:
		packet_element * element(int header);

	protected:
		std::map<int, packet_element> packet_map_;
		packet_element * current_packet_;
		uint32_t start_time_;
};

class packet_info_cg : public packet_info
{
	public:
		packet_info_cg();
		~packet_info_cg() override;
};

// PacketInfo P2P 
class packet_info_gg : public packet_info
{
	public:
		packet_info_gg();
		~packet_info_gg() override;
};

/// Implemented in input_udp.cpp
}
