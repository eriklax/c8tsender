#include "playlist.hpp"
#include <libgen.h>
#include <uuid/uuid.h>
#include <random>

PlaylistItem::PlaylistItem(const std::string& path)
{
	m_path = path;
	m_name = basename((char*)path.c_str());
	uuid_t out;
	uuid_generate(out);
	uuid_string_t str;
	uuid_unparse(out, str);
	m_uuid = str;
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
}

void Playlist::insert(const PlaylistItem& item)
{
	m_items.push_back(item);
}

bool Playlist::remove(const std::string& uuid)
{
	auto ptr = std::remove_if(m_items.begin(), m_items.end(),
			[&uuid](PlaylistItem const& item) {
				return item.getUUID() == uuid;
			});
	m_items.erase(ptr, m_items.end());
	return true;
}

PlaylistItem Playlist::getTrack(const std::string& uuid)
{
	auto ptr = std::find_if(m_items.begin(), m_items.end(),
			[&uuid](PlaylistItem const& item) {
				return item.getUUID() == uuid;
			});
	if (ptr == m_items.end())
		throw std::runtime_error("track not found");
	return *ptr;
}

PlaylistItem Playlist::getNextTrack(const std::string& uuid)
{
	if (m_items.empty())
		throw std::runtime_error("playlist is empty");

	if (m_shuffle && !m_repeat)
	{
		std::random_device rd;
		std::default_random_engine e1(rd());
		std::uniform_int_distribution<size_t> uniform_dist(0, m_items.size() - 1);
		return m_items[uniform_dist(e1)];
	}

	auto ptr = std::find_if(m_items.begin(), m_items.end(),
			[&uuid](PlaylistItem const& item) {
				return item.getUUID() == uuid;
			});
	if (ptr == m_items.end())
		return m_items[0];
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

void Playlist::setRepeat(bool value)
{
	m_repeat = value;
}

void Playlist::setRepeatAll(bool value)
{
	m_repeatall = value;
}

void Playlist::setShuffle(bool value)
{
	m_shuffle = value;
}
