#include "webserver.hpp"
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <signal.h>
#include <microhttpd.h>
#include <json/json.h>
#include <fstream>
#include <streambuf>
#include <syslog.h>

Webserver::Webserver(unsigned short port, ChromeCast& sender, Playlist& playlist)
: m_port(port)
, m_sender(sender)
, m_playlist(playlist)
{
	mp_d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
			port,
			NULL,
			NULL,
			&Webserver::_REST_API,
			this,
			MHD_OPTION_END);
	if (!mp_d)
		throw std::runtime_error("MHD_start_daemon failed");
}

Webserver::~Webserver()
{
	MHD_stop_daemon(mp_d);
}

int mhd_queue_json(struct MHD_Connection* connection, int status_code, const Json::Value& json)
{
	Json::FastWriter fw;
	fw.omitEndingLineFeed();
	std::string data = fw.write(json);
	MHD_Response* response = MHD_create_response_from_buffer(data.size(),
			(void*)data.c_str(),
			MHD_RESPMEM_MUST_COPY);
	MHD_add_response_header(response, "Content-Type", "application/json");
	int ret = MHD_queue_response(connection,
			status_code,
			response);
	MHD_destroy_response(response);
	return ret;
}

struct PostRequest
{
	bool recving;
	std::string read_post_data;
};

int Webserver::REST_API(struct MHD_Connection* connection,
		const char* url,
		const char* method,
		const char* version,
		const char* upload_data,
		size_t* upload_data_size,
		void** ptr)
{
	std::string postdata;
	if (strcmp(method, MHD_HTTP_METHOD_POST) == 0)
	{
		PostRequest* request = static_cast<PostRequest*>(*ptr);
		if (!request) {
			request = new PostRequest;
			request->recving = false;
			*ptr = request;
		}
		if (!request->recving) {
			request->recving = true;
			return MHD_YES;
		} else {
			if (*upload_data_size != 0) {
				request->read_post_data.append(upload_data, *upload_data_size);
				*upload_data_size = 0;
				return MHD_YES;
			} else {
				postdata += request->read_post_data;
				delete request;
				request = NULL;
				*ptr = NULL;
			}
		}
	}

	syslog(LOG_DEBUG, "%s %s", method, url);

	if (strcmp(method, "POST") == 0)
	{
		if (strcmp(url, "/playlist") == 0)
			return POST_playlist(connection, postdata);
		return MHD_NO;
	}
	if (strcmp(method, "GET") == 0)
	{
		if (strcmp(url, "/") == 0)
			return GET_file(connection, "htdocs/index.html", "text/html");
		if (strcmp(url, "/bootstrap.min.css") == 0)
			return GET_file(connection, "htdocs/bootstrap.min.css", "text/css");
		if (strcmp(url, "/fonts/glyphicons-halflings-regular.ttf") == 0)
			return GET_file(connection, "htdocs/glyphicons-halflings-regular.ttf", "application/x-font-ttf");
		if (strcmp(url, "/fonts/glyphicons-halflings-regular.woff") == 0)
			return GET_file(connection, "htdocs/glyphicons-halflings-regular.woff", "application/octet-stream");
		if (strcmp(url, "/bootstrap-theme.min.css") == 0)
			return GET_file(connection, "htdocs/bootstrap-theme.min.css", "text/css");
		if (strcmp(url, "/bootstrap.min.js") == 0)
			return GET_file(connection, "htdocs/bootstrap.min.js", "text/javascript");
		if (strcmp(url, "/jquery-2.1.1.min.js") == 0)
			return GET_file(connection, "htdocs/jquery-2.1.1.min.js", "text/javascript");
		if (strncmp(url, "/play/", 6) == 0)
			return GET_play(connection, url + 6);
		if (strcmp(url, "/next") == 0)
			return GET_next(connection);
		if (strcmp(url, "/streaminfo") == 0)
			return GET_streaminfo(connection);
		if (strcmp(url, "/pause") == 0)
			return GET_pause(connection);
		if (strcmp(url, "/resume") == 0)
			return GET_resume(connection);
		if (strcmp(url, "/stop") == 0)
			return GET_stop(connection);
		if (strcmp(url, "/playlist") == 0)
			return GET_playlist(connection);
		if (strncmp(url, "/playlist/repeat/", 17) == 0)
			return GET_playlist_repeat(connection, strcmp(url + 17, "1") == 0);
		if (strncmp(url, "/playlist/repeatall/", 20) == 0)
			return GET_playlist_repeatall(connection, strcmp(url + 20, "1") == 0);
		if (strncmp(url, "/playlist/shuffle/", 18) == 0)
			return GET_playlist_shuffle(connection, strcmp(url + 18, "1") == 0);
		if (strncmp(url, "/stream/", 8) == 0)
			return GET_stream(connection, url + 8);
	}
	return MHD_NO;
}

bool Webserver::isPrivileged(struct MHD_Connection* connection)
{
	// check if connection from 127.0.0.1
	struct sockaddr* s = MHD_get_connection_info(connection,
			MHD_CONNECTION_INFO_CLIENT_ADDRESS)->client_addr;
	return (s->sa_family == AF_INET && ((struct sockaddr_in*)s)->sin_addr.s_addr == 0x0100007F);
}

int Webserver::GET_file(struct MHD_Connection* connection, const std::string& file, const std::string& contentType)
{
	std::ifstream t(file);
	std::string str((std::istreambuf_iterator<char>(t)),
			std::istreambuf_iterator<char>());
	MHD_Response* response = MHD_create_response_from_buffer(str.size(),
			(void*)str.c_str(),
			MHD_RESPMEM_MUST_COPY);
	MHD_add_response_header(response, "Content-Type", contentType.c_str());
	int ret = MHD_queue_response(connection,
			MHD_HTTP_OK,
			response);
	MHD_destroy_response(response);
	return ret;
}

int Webserver::POST_playlist(struct MHD_Connection* connection, const std::string& data)
{
	if (!isPrivileged(connection))
		return mhd_queue_json(connection, 403, Json::Value());

	Json::Value json;
	{
		std::lock_guard<std::mutex> lock(m_playlist.getMutex());

		PlaylistItem track(data);
		m_playlist.insert(track);

		json["uuid"] = track.getUUID();
	}
	return mhd_queue_json(connection, MHD_HTTP_OK, json);
}

int Webserver::GET_playlist(struct MHD_Connection* connection)
{
	Json::Value json;
	{
		std::lock_guard<std::mutex> lock(m_playlist.getMutex());

		json["uuid"] = m_playlist.getUUID();
		json["repeat"] = m_playlist.getRepeat();
		json["repeatall"] = m_playlist.getRepeatAll();
		json["shuffle"] = m_playlist.getShuffle();
		Json::Value tracklist(Json::arrayValue);
		for (auto& track : m_playlist.getTracks())
		{
			Json::Value t;
			t["name"] = track.getName();
			t["uuid"] = track.getUUID();
			tracklist.append(t);
		}
		json["tracks"] = tracklist;
	}
	return mhd_queue_json(connection, MHD_HTTP_OK, json);
}

int Webserver::GET_playlist_repeat(struct MHD_Connection* connection, bool value)
{
	Json::Value json;
	{
		std::lock_guard<std::mutex> lock(m_playlist.getMutex());

		m_playlist.setRepeat(value);
		json["repeat"] = m_playlist.getRepeat();
	}
	return mhd_queue_json(connection, MHD_HTTP_OK, json);
}

int Webserver::GET_playlist_repeatall(struct MHD_Connection* connection, bool value)
{
	Json::Value json;
	{
		std::lock_guard<std::mutex> lock(m_playlist.getMutex());

		m_playlist.setRepeatAll(value);
		json["repeatall"] = m_playlist.getRepeatAll();
	}
	return mhd_queue_json(connection, MHD_HTTP_OK, json);
}

int Webserver::GET_playlist_shuffle(struct MHD_Connection* connection, bool value)
{
	Json::Value json;
	{
		std::lock_guard<std::mutex> lock(m_playlist.getMutex());

		m_playlist.setShuffle(value);
		json["shuffle"] = m_playlist.getShuffle();
	}
	return mhd_queue_json(connection, MHD_HTTP_OK, json);
}

int Webserver::GET_pause(struct MHD_Connection* connection)
{
	if (!m_sender.pause())
		return mhd_queue_json(connection, 500, Json::Value());
	return mhd_queue_json(connection, MHD_HTTP_OK, Json::Value());
}

int Webserver::GET_resume(struct MHD_Connection* connection)
{
	if (!m_sender.play())
		return mhd_queue_json(connection, 500, Json::Value());
	return mhd_queue_json(connection, MHD_HTTP_OK, Json::Value());
}

int Webserver::GET_stop(struct MHD_Connection* connection)
{
	if (!m_sender.stop())
		return mhd_queue_json(connection, 500, Json::Value());
	return mhd_queue_json(connection, MHD_HTTP_OK, Json::Value());
}

int Webserver::GET_play(struct MHD_Connection* connection, const std::string& uuid)
{
	std::string name;
	try {
		std::lock_guard<std::mutex> lock(m_playlist.getMutex());

		const PlaylistItem& track = m_playlist.getTrack(uuid);
		name = track.getName();
	} catch (std::runtime_error& e) {
		Json::Value json;
		json["error"] = e.what();
		return mhd_queue_json(connection, 500, json);
	}
	if (!m_sender.load(
			"http://" + m_sender.getSocketName() + ":" + std::to_string(m_port) + "/stream/" + uuid,
			name, uuid))
		return mhd_queue_json(connection, 500, Json::Value());
	return mhd_queue_json(connection, MHD_HTTP_OK, Json::Value());
}

int Webserver::GET_next(struct MHD_Connection* connection)
{
	Json::Value json;
	std::string name, uuid;
	try {
		std::lock_guard<std::mutex> lock(m_playlist.getMutex());

		const PlaylistItem& track = m_playlist.getNextTrack(m_sender.getUUID());
		name = track.getName();
		uuid = track.getUUID();
		json["uuid"] = uuid;
	} catch (std::runtime_error& e) {
		Json::Value json;
		json["error"] = e.what();
		return mhd_queue_json(connection, 500, json);
	}
	if (!m_sender.load(
			"http://" + m_sender.getSocketName() + ":" + std::to_string(m_port) + "/stream/" + uuid,
			name, uuid))
		return mhd_queue_json(connection, 500, Json::Value());
	return mhd_queue_json(connection, MHD_HTTP_OK, json);
}

struct mhd_forkctx
{
	pid_t pid;
	int fd;
};

void mhd_forkctx_clean(void* cls)
{
	mhd_forkctx* f = static_cast<mhd_forkctx*>(cls);
	int status;
	kill(f->pid, SIGKILL);
	waitpid(f->pid, &status, 0);
	close(f->fd);
	delete f;
}

ssize_t mhd_forkctx_read(void* cls, uint64_t pos, char* buf, size_t max)
{
	mhd_forkctx* f = static_cast<mhd_forkctx*>(cls);
	int r = read(f->fd, buf, max);
	if (r == 0)
		return MHD_CONTENT_READER_END_OF_STREAM;
	return r;
}

int Webserver::GET_stream(struct MHD_Connection* connection, const std::string& uuid)
{
	std::string path;
	try {
		std::lock_guard<std::mutex> lock(m_playlist.getMutex());
		const PlaylistItem& track = m_playlist.getTrack(uuid);
		path = track.getPath();
	} catch (std::runtime_error& e) {
		Json::Value json;
		json["error"] = e.what();
		return mhd_queue_json(connection, 500, json);
	}

	std::vector<const char*> cbuf;
	cbuf.push_back("ffmpeg");
	cbuf.push_back("-y");
	cbuf.push_back("-i"); cbuf.push_back(path.c_str());
	cbuf.push_back("-vcodec"); cbuf.push_back("copy");
	cbuf.push_back("-acodec"); cbuf.push_back("aac");
	cbuf.push_back("-strict"); cbuf.push_back("-2");
	cbuf.push_back("-f"); cbuf.push_back("matroska");
	cbuf.push_back("-");
	cbuf.push_back(0);

	int mypipe[2];
	pipe(mypipe);
	pid_t pid = fork();
	if (pid == (pid_t)0)
	{
		// Include current PWD
		char* env;
		asprintf(&env, ".:%s", getenv("PATH"));
		setenv("PATH", env, 1);

		close(mypipe[0]);
		close(fileno(stderr));
		dup2(mypipe[1], fileno(stdout));
		execvp(cbuf[0], (char* const*)&cbuf[0]);
		_exit(1);
	}
	close(mypipe[1]);
	mhd_forkctx* f = new mhd_forkctx;
	f->pid = pid;
	f->fd = mypipe[0];
	MHD_Response* response = MHD_create_response_from_callback(-1, 8192, &mhd_forkctx_read, f, &mhd_forkctx_clean);
	MHD_add_response_header(response, "Content-Type", "video/x-matroska");
	int ret = MHD_queue_response(connection,
			MHD_HTTP_OK,
			response);
	MHD_destroy_response(response);
	return ret;
}

int Webserver::GET_streaminfo(struct MHD_Connection* connection)
{
	std::lock_guard<std::mutex> lock(m_playlist.getMutex());

	Json::Value json;
	json["uuid"] = m_sender.getUUID();
	json["playerstate"] = m_sender.getPlayerState();
	json["playlist"] = m_playlist.getUUID();
	return mhd_queue_json(connection, MHD_HTTP_OK, json);
}
