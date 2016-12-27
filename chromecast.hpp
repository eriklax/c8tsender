#ifndef _CHROMECAST_HPP_
#define _CHROMECAST_HPP_

#include <Security/SecureTransport.h>
#include <json/json.h>
#include <thread>
#include <string>
#include <map>

class ChromeCast {
	public:
		ChromeCast(const std::string& ip);
		~ChromeCast();
		bool init();

		void setMediaStatusCallback(std::function<void(const std::string&,
					const std::string&, const std::string&)> func);
		bool load(const std::string& url, const std::string& title, const std::string& uuid);
		bool play();
		bool pause();
		bool stop();
		bool setSubtitles(bool status);
		bool setVolume(double level);
		bool setMuted(bool muted);
		double getVolume() const;
		bool getMuted() const;
		void setSubtitleSettings(bool status);

		const std::string& getUUID() const;
		const std::string& getPlayerState() const;
		double getPlayerCurrentTime() const;
		bool hasSubtitles() const;
		std::string getSocketName() const;
	private:
		bool connect();
		void disconnect();

		Json::Value send(const std::string& namespace_, const Json::Value& payload, const std::string& destination_id = "");
		void _read();
		void _release_waiters();

		std::string m_ip;
		int m_s;
		SSLContextRef m_ssl;
		std::mutex m_mutex;
		std::mutex m_ssl_mutex;
		std::thread m_tread;
		std::map<unsigned int, std::pair<std::condition_variable*, Json::Value>> m_wait;

		std::string m_uuid;
		std::string m_player_state;
		double m_player_current_time;
		time_t m_player_current_time_update;
		bool m_subtitles = false;
		double m_volume;
		bool m_muted;
		std::string m_session_id;
		unsigned int m_media_session_id;
		unsigned int _request_id();
		int m_request_id = 0;
		bool m_init = false;
		std::string m_source_id = "sender-0";
		std::string m_destination_id = "receiver-0";

		std::function<void(const std::string&,
				const std::string&, const std::string&)> m_mediaStatusCallback;
};

#endif
