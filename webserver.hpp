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
