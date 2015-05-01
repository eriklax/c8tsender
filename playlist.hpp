#ifndef _PLAYLIST_HPP_
#define _PLAYLIST_HPP_

#include <string>
#include <vector>
#include <mutex>

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
		void queueTrack(const std::string& uuid);
		const PlaylistItem& getTrack(const std::string& uuid) const;
		const PlaylistItem& getNextTrack(const std::string& uuid = std::string()) const;

		bool getRepeat() const;
		bool getRepeatAll() const;
		bool getShuffle() const;
		const std::vector<PlaylistItem>& getTracks() const;
		const std::vector<std::string>& getQueue() const;
		const std::string& getUUID() const;

		void setRepeat(bool value);
		void setRepeatAll(bool value);
		void setShuffle(bool value);

		std::mutex& getMutex();
	private:
		bool m_repeat;
		bool m_repeatall;
		bool m_shuffle;
		mutable std::vector<std::string> m_queue;
		std::vector<PlaylistItem> m_items;
		mutable std::string m_uuid;
		std::mutex m_mutex;
};

#endif
