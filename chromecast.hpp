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

		void setMediaStatusCallback(std::function<void(const std::string&,
					const std::string&, const std::string&)> func);
		void init();
		void load(const std::string& url, const std::string& title, const std::string& uuid);
		void play();
		void pause();
		void stop();

		const std::string& getUUID() const;
		const std::string& getPlayerState() const;
		std::string getSocketName() const;
	private:
		Json::Value send(const std::string& namespace_, const Json::Value& payload);
		void _read();

		int m_s;
		SSL* m_ssls;
		std::mutex m_mutex;
		std::thread m_tread;
		std::map<unsigned int, std::pair<std::condition_variable*, Json::Value>> m_wait;

		std::string m_uuid;
		std::string m_player_state;
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
