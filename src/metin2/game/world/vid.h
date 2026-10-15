#pragma once

#include <cstdint>

namespace metin2::game::inline world {

class vid
{
	public:
		vid() : id_(0), crc_(0)
		{
		}

		vid(uint32_t id, uint32_t crc)
		{
			id_ = id;
			crc_ = crc;
		}

		vid(const vid &rvid)
		{
			*this = rvid;
		}

		auto operator = (const vid & rhs) -> const vid &
		{
			id_ = rhs.id_;
			crc_ = rhs.crc_;
			return *this;
		}

		auto operator == (const vid & rhs) const -> bool
		{
			return (id_ == rhs.id_) && (crc_ == rhs.crc_);
		}

		auto operator != (const vid & rhs) const -> bool
		{
			return !(*this == rhs);
		}

		operator uint32_t() const
		{
			return id_;
		}

		void reset()
		{
			id_ = 0, crc_ = 0;
		}

	private:
		uint32_t id_;
		uint32_t crc_;
};
}
