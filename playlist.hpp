#ifndef _PLAYLIST_HPP_
#define _PLAYLIST_HPP_

#include <string>
#include <vector>

class PlaylistItem {
	public:
		PlaylistItem(const std::string& path);

		const std::string& getName() const;
		const std::string& getPath() const;
		const std::string& getUUID() const;
	private:
		std::string m_uuid;
		std::string m_name;
		std::string m_path;
};

class Playlist {
	public:
		Playlist();

		void insert(const PlaylistItem& item);
		bool remove(const std::string& uuid);
		PlaylistItem getTrack(const std::string& uuid) const;
		PlaylistItem getNextTrack(const std::string& uuid) const;

		bool getRepeat() const;
		bool getRepeatAll() const;
		bool getShuffle() const;
		const std::vector<PlaylistItem>& getTracks() const;
		const std::string& getUUID() const;

		void setRepeat(bool value);
		void setRepeatAll(bool value);
		void setShuffle(bool value);
	private:
		bool m_repeat;
		bool m_repeatall;
		bool m_shuffle;
		std::vector<PlaylistItem> m_items;
		std::string m_uuid;
};

#endif
