#ifndef _CHROMECAST_HPP_
#define _CHROMECAST_HPP_

#include <openssl/ssl.h>
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

		const std::string& getUUID() const;
		const std::string& getPlayerState() const;
		bool hasSubtitles() const;
		std::string getSocketName() const;
	private:
		bool connect();
		void disconnect();

		Json::Value send(const std::string& namespace_, const Json::Value& payload);
		void _read();
		void _release_waiters();

		std::string m_ip;
		int m_s;
		SSL* m_ssl;
		SSL_CTX* m_ssl_ctx;
		std::mutex m_mutex;
		std::mutex m_ssl_mutex;
		std::thread m_tread;
		std::map<unsigned int, std::pair<std::condition_variable*, Json::Value>> m_wait;

		std::string m_uuid;
		std::string m_player_state;
		bool m_subtitles = false;
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
