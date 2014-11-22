#ifndef _WEBSERVER_HPP_
#define _WEBSERVER_HPP_

#include "playlist.hpp"
#include "chromecast.hpp"
#include <microhttpd.h>

class Webserver {
	public:
		Webserver(unsigned short port, ChromeCast& sender, Playlist& playlist);
		~Webserver();	

	private:
		int GET_file(struct MHD_Connection* connection, const std::string& file, const std::string& contentType);
		int POST_playlist(struct MHD_Connection* connection, const std::string& data);
		int GET_playlist(struct MHD_Connection* connection);
		int GET_playlist_repeat(struct MHD_Connection* connection, bool value);
		int GET_playlist_repeatall(struct MHD_Connection* connection, bool value);
		int GET_playlist_shuffle(struct MHD_Connection* connection, bool value);
		int GET_pause(struct MHD_Connection* connection);
		int GET_resume(struct MHD_Connection* connection);
		int GET_stop(struct MHD_Connection* connection);
		int GET_play(struct MHD_Connection* connection, const std::string& uuid, time_t startTime = 0);
		int GET_next(struct MHD_Connection* connection);
		int GET_stream(struct MHD_Connection* connection, const std::string& uuid, time_t startTime = 0);
		int GET_streaminfo(struct MHD_Connection* connection);

		bool isPrivileged(struct MHD_Connection* connection);

		int REST_API(struct MHD_Connection* connection,
				const char* url,
				const char* method,
				const char* version,
				const char* upload_data,
				size_t* upload_data_size,
				void** ptr);

		static int _REST_API(void* cls,
				struct MHD_Connection* connection,
				const char* url,
				const char* method,
				const char* version,
				const char* upload_data,
				size_t* upload_data_size,
				void** ptr) {
			return (static_cast<Webserver*>(cls)->REST_API)(connection, url, method, version, upload_data, upload_data_size, ptr);
		}

		ChromeCast& m_sender;
		Playlist& m_playlist;
		
		short int m_port;
		struct MHD_Daemon* mp_d;
};

#endif
