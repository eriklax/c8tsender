#include "playlist.hpp"
#include <libgen.h>
#include <uuid/uuid.h>
#include <random>

std::string uuidgen()
{
	uuid_t out;
	uuid_generate(out);
	uuid_string_t str;
	uuid_unparse(out, str);
	return str;
}

PlaylistItem::PlaylistItem(const std::string& path)
{
	m_path = path;
	m_name = basename((char*)path.c_str());
	if (m_name.find_last_of(".") != std::string::npos)
		m_name.erase(m_name.find_last_of("."), std::string::npos);
	m_uuid = uuidgen();
}

const std::string& PlaylistItem::getName() const
{
	return m_name;
}

const std::string& PlaylistItem::getPath() const
{
	return m_path;
}

const std::string& PlaylistItem::getUUID() const
{
	return m_uuid;
}

Playlist::Playlist()
: m_repeat(false)
, m_repeatall(false)
, m_shuffle(false)
{
	m_uuid = uuidgen();
}

void Playlist::insert(const PlaylistItem& item)
{
	m_items.push_back(item);
	m_uuid = uuidgen();
}

bool Playlist::remove(const std::string& uuid)
{
	auto ptr = std::remove_if(m_items.begin(), m_items.end(),
			[&uuid](PlaylistItem const& item) {
				return item.getUUID() == uuid;
			});
	if (ptr == m_items.end())
		return false;

	auto ptr2 = std::remove_if(m_queue.begin(), m_queue.end(),
			[&uuid](std::string const& item) {
				return item == uuid;
			});
	if (ptr2 != m_queue.end())
		m_queue.erase(ptr2, m_queue.end());

	m_items.erase(ptr, m_items.end());
	m_uuid = uuidgen();
	return true;
}

void Playlist::queueTrack(const std::string& uuid)
{
	m_queue.push_back(uuid);
	m_uuid = uuidgen();
}

const PlaylistItem& Playlist::getTrack(const std::string& uuid) const
{
	auto ptr = std::find_if(m_items.begin(), m_items.end(),
			[&uuid](PlaylistItem const& item) {
				return item.getUUID() == uuid;
			});
	if (ptr == m_items.end())
		throw std::runtime_error("track not found");
	return *ptr;
}

const PlaylistItem& Playlist::getNextTrack(const std::string& uuid) const
{
	if (m_items.empty())
		throw std::runtime_error("playlist is empty");

	while (!m_queue.empty())
	{
		std::string item = *m_queue.begin();
		m_queue.erase(m_queue.begin());
		try {
			auto& track = getTrack(item);
			m_uuid = uuidgen();
			return track;
		} catch (...) {
			// next
		}
	}

	if (m_shuffle && !m_repeat)
	{
		std::random_device rd;
		std::default_random_engine e1(rd());
		std::uniform_int_distribution<size_t> uniform_dist(0, m_items.size() - 1);
		size_t choosen = uniform_dist(e1);

		// Improve the shuffle experience for users who does not appreciate true randomness
		if (m_items.size() > 1)
			while (m_items[choosen].getUUID() == uuid)
				choosen = uniform_dist(e1);

		return m_items[choosen];
	}

	auto ptr = std::find_if(m_items.begin(), m_items.end(),
			[&uuid](PlaylistItem const& item) {
				return item.getUUID() == uuid;
			});
	if (ptr == m_items.end())
		return m_items[0];
	if (m_repeat)
		return *ptr;
	++ptr;
	if (ptr == m_items.end()) {
		if (!m_repeatall)
			throw std::runtime_error("playlist done");
		ptr = m_items.begin();
	}
	return *ptr;
}

bool Playlist::getRepeat() const
{
	return m_repeat;
}

bool Playlist::getRepeatAll() const
{
	return m_repeatall;
}

bool Playlist::getShuffle() const
{
	return m_shuffle;
}

const std::vector<PlaylistItem>& Playlist::getTracks() const
{
	return m_items;
}

const std::vector<std::string>& Playlist::getQueue() const
{
	return m_queue;
}

const std::string& Playlist::getUUID() const
{
	return m_uuid;
}

void Playlist::setRepeat(bool value)
{
	if (m_repeat != value)
		m_uuid = uuidgen();
	m_repeat = value;
}

void Playlist::setRepeatAll(bool value)
{
	if (m_repeatall != value)
		m_uuid = uuidgen();
	m_repeatall = value;
}

void Playlist::setShuffle(bool value)
{
	if (m_shuffle != value)
		m_uuid = uuidgen();
	m_shuffle = value;
}

std::mutex& Playlist::getMutex()
{
	return m_mutex;
}
