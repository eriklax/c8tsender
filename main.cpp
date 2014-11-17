#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <signal.h>
#include <microhttpd.h>
#include <json/json.h>
#include <fstream>
#include <streambuf>
#include "playlist.hpp"
#include "chromecast.hpp"
#include "cast_channel.pb.h"

Playlist playlist;
std::string uuid;

struct mhd_forkctx
{
	pid_t pid;
	int fd;
};

void mhd_forkctx_clean(void* cls)
{
	mhd_forkctx* f = (mhd_forkctx*)cls;
	int status;
	kill(f->pid, SIGKILL);
	waitpid(f->pid, &status, 0);
	close(f->fd);
	delete f;
}

ssize_t mhd_forkctx_read(void* cls, uint64_t pos, char* buf, size_t max)
{
	mhd_forkctx* f = (mhd_forkctx*)cls;
	int r = read(f->fd, buf, max);
	if (r == 0)
		return MHD_CONTENT_READER_END_OF_STREAM;
	return r;
}

int mhd_queue_json(struct MHD_Connection* connection, int status_code, Json::Value& json)
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

struct ConnectionData
{
	bool recving;
	std::string read_post_data;
};

static int rest_api(void* cls,
		struct MHD_Connection* connection,
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
		ConnectionData* connection_data = static_cast<ConnectionData*>(*ptr);
		if (!connection_data) {
			connection_data = new ConnectionData;
			connection_data->recving = false;
			*ptr = connection_data;
		}
		if (!connection_data->recving) {
			connection_data->recving = true;
			return MHD_YES;
		} else {
			if (*upload_data_size != 0) {
				connection_data->read_post_data.append(upload_data, *upload_data_size);
				*upload_data_size = 0;
				return MHD_YES;
			} else {
				postdata += connection_data->read_post_data;
				delete connection_data;
				connection_data = NULL;
				*ptr = NULL;
			}
		}
	}

	printf("%s %s\n", method, url);

	if (strcmp(method, "POST") == 0)
	{
		if (strcmp(url, "/playlist") == 0)
		{
			struct sockaddr* s = MHD_get_connection_info(connection,
			        MHD_CONNECTION_INFO_CLIENT_ADDRESS)->client_addr;
			Json::Value json;

			// Only localhost...
			if (s->sa_family == AF_INET && ((struct sockaddr_in*)s)->sin_addr.s_addr == 0x0100007F)
			{
				PlaylistItem track(postdata);
				playlist.insert(track);
				json["uuid"] = track.getUUID();
				return mhd_queue_json(connection, MHD_HTTP_OK, json);
			}
			return mhd_queue_json(connection, 403, json);
		}
	}

	if (strcmp(method, "GET") != 0)
		return MHD_NO;

	if (strcmp(url, "/") == 0)
	{
		std::ifstream t("index.html");
		std::string str((std::istreambuf_iterator<char>(t)),
				std::istreambuf_iterator<char>());
		MHD_Response* response = MHD_create_response_from_buffer(str.size(),
				(void*)str.c_str(),
				MHD_RESPMEM_MUST_COPY);
		MHD_add_response_header(response, "Content-Type", "text/html");
		int ret = MHD_queue_response(connection,
				MHD_HTTP_OK,
				response);
		MHD_destroy_response(response);
		return ret;
	}

	if (strncmp(url, "/play/", 6) == 0)
	{
		try {
			PlaylistItem track = playlist.getTrack(url + 6);
			uuid = track.getUUID();
			static_cast<ChromeCast*>(cls)->load(
					"http://" + static_cast<ChromeCast*>(cls)->getSocketName() + ":8080/stream/" + track.getUUID(),
					track.getName(), track.getUUID());
			Json::Value json;
			return mhd_queue_json(connection, MHD_HTTP_OK, json);
		} catch (std::runtime_error& e) {
			Json::Value json;
			json["error"] = e.what();
			return mhd_queue_json(connection, 500, json);
		}
	}
	if (strcmp(url, "/next") == 0)
	{
		try {
			PlaylistItem track = playlist.getNextTrack(uuid);
			uuid = track.getUUID();
			static_cast<ChromeCast*>(cls)->load(
					"http://" + static_cast<ChromeCast*>(cls)->getSocketName() + ":8080/stream/" + track.getUUID(),
					track.getName(), track.getUUID());
			Json::Value json;
			return mhd_queue_json(connection, MHD_HTTP_OK, json);
		} catch (std::runtime_error& e) {
			Json::Value json;
			json["error"] = e.what();
			return mhd_queue_json(connection, 500, json);
		}
	}
	if (strcmp(url, "/streamuuid") == 0)
	{
		Json::Value json;
		json["uuid"] = uuid;
		return mhd_queue_json(connection, MHD_HTTP_OK, json);
	}
	if (strcmp(url, "/pause") == 0)
	{
		static_cast<ChromeCast*>(cls)->pause();
		Json::Value json;
		return mhd_queue_json(connection, MHD_HTTP_OK, json);
	}
	if (strcmp(url, "/resume") == 0)
	{
		static_cast<ChromeCast*>(cls)->play();
		Json::Value json;
		return mhd_queue_json(connection, MHD_HTTP_OK, json);
	}
	if (strcmp(url, "/stop") == 0)
	{
		static_cast<ChromeCast*>(cls)->stop();
		Json::Value json;
		return mhd_queue_json(connection, MHD_HTTP_OK, json);
	}
	if (strcmp(url, "/playlist") == 0)
	{
		Json::Value json;
		json["repeat"] = playlist.getRepeat();
		json["repeatall"] = playlist.getRepeatAll();
		json["shuffle"] = playlist.getShuffle();
		Json::Value tracklist(Json::arrayValue);
		for (auto& track : playlist.getTracks())
		{
			Json::Value t;
			t["name"] = track.getName();
			t["uuid"] = track.getUUID();
			tracklist.append(t);
		}
		json["tracks"] = tracklist;
		return mhd_queue_json(connection, MHD_HTTP_OK, json);
	}
	if (strncmp(url, "/playlist/repeat/", 17) == 0)
	{
		playlist.setRepeat(strcmp(url + 17, "1") == 0);
		Json::Value json;
		json["repeat"] = playlist.getRepeat();
		return mhd_queue_json(connection, MHD_HTTP_OK, json);
	}
	if (strncmp(url, "/playlist/repeatall/", 20) == 0)
	{
		playlist.setRepeatAll(strcmp(url + 20, "1") == 0);
		Json::Value json;
		json["repeatall"] = playlist.getRepeatAll();
		return mhd_queue_json(connection, MHD_HTTP_OK, json);
	}
	if (strncmp(url, "/playlist/shuffle/", 18) == 0)
	{
		playlist.setShuffle(strcmp(url + 18, "1") == 0);
		Json::Value json;
		json["shuffle"] = playlist.getShuffle();
		return mhd_queue_json(connection, MHD_HTTP_OK, json);
	}

	if (strncmp(url, "/stream/", 8) == 0)
	{
		std::string path;
		try {
			PlaylistItem track = playlist.getTrack(url + 8);
			path = track.getPath();
		} catch (std::runtime_error& e) {
			Json::Value json;
			json["error"] = e.what();
			return mhd_queue_json(connection, 500, json);
		}

		std::vector<const char*> cbuf;
		cbuf.push_back("./ffmpeg");
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

	return MHD_NO;
}

int main(int argc, char* argv[]) {
	if (argc < 2)
		return 1;
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	OpenSSL_add_ssl_algorithms();
 	SSL_load_error_strings();
	ChromeCast tv(argv[1]);
	tv.init();
	tv.setMediaStatusCallback([&tv](const std::string& playerState,
			const std::string& idleReason, const std::string& customData) -> void {
		printf("mediastatus: %s %s %s\n", playerState.c_str(), idleReason.c_str(), customData.c_str());
		if (!customData.empty())
			uuid = customData;
		if (playerState == "IDLE") {
			std::string _uuid = uuid;
			uuid = "";
			if (idleReason == "FINISHED") {
				try {
					PlaylistItem track = playlist.getNextTrack(_uuid);
					uuid = track.getUUID();
					std::thread foo([&tv, track](){
						tv.load("http://" + tv.getSocketName() + ":8080/stream/" + track.getUUID(),
							track.getName(), track.getUUID());
					});
					foo.detach();
				} catch (...) {
					// ...
				}
			}
		}
	});

	struct MHD_Daemon* d;
	d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
			8080,
			NULL,
			NULL,
			&rest_api,
			(void*)&tv,
			MHD_OPTION_END);
	if (d == NULL)
		return 1;

	while (1) sleep(3600);
	MHD_stop_daemon(d);
}
