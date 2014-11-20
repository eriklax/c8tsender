#include "playlist.hpp"
#include "chromecast.hpp"
#include "webserver.hpp"
#include "cast_channel.pb.h"
#include <syslog.h>
#include <getopt.h>

void usage();
extern char* __progname;

int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	OpenSSL_add_ssl_algorithms();
 	SSL_load_error_strings();
	signal(SIGPIPE, SIG_IGN);
	openlog(NULL, LOG_PID | LOG_PERROR, LOG_DAEMON);

	std::string ip;
	unsigned short port = 8080;
	Playlist playlist;

	static struct option longopts[] = {
		{ "chromecast", required_argument, NULL, 'c' },
		{ "port", required_argument, NULL, 'P' },
		{ "shuffle", no_argument, NULL, 's' },
		{ "repeat", no_argument, NULL, 'r' },
		{ "repeat-all", no_argument, NULL, 'R' },
		{ NULL, 0, NULL, 0 }
	};

	int ch;
	while ((ch = getopt_long(argc, argv, "c:P:srR", longopts, NULL)) != -1) {
		switch (ch) {
			case 'c':
				ip = optarg;
				break;
			case 'P':
				port = strtoul(optarg, NULL, 10);
				break;
			case 's':
				playlist.setShuffle(true);
				break;
			case 'r':
				playlist.setRepeat(true);
				break;
			case 'R':
				playlist.setRepeatAll(true);
				break;
			default:
			case 'h':
				usage();
		}
	}

	if (ip.empty())
		usage();

	ChromeCast chromecast(ip);
	chromecast.init();
	chromecast.setMediaStatusCallback([&chromecast, &playlist, port](const std::string& playerState,
			const std::string& idleReason, const std::string& uuid) -> void {
		syslog(LOG_DEBUG, "mediastatus: %s %s %s", playerState.c_str(), idleReason.c_str(), uuid.c_str());
		if (playerState == "IDLE") {
			if (idleReason == "FINISHED") {
				try {
					std::lock_guard<std::mutex> lock(playlist.getMutex());
					PlaylistItem track = playlist.getNextTrack(uuid);
					std::thread foo([&chromecast, port, track](){
						chromecast.load("http://" + chromecast.getSocketName() + ":" + std::to_string(port) + "/stream/" + track.getUUID(),
							track.getName(), track.getUUID());
					});
					foo.detach();
				} catch (...) {
					// ...
				}
			}
		}
	});
	Webserver http(port, chromecast, playlist);
	while (1) sleep(3600);
	return 0;
}

void usage()
{
	printf("%s --chromecast <ip> [ --port <number> ] [ --playlist <path> ] [ --shuffle ] [ --repeat ] [ --repeat-all ]\n", __progname);
	exit(1);
}
