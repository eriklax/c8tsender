#include "chromecast.hpp"
#include "cast_channel.pb.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <syslog.h>

ChromeCast::ChromeCast(const std::string& ip)
: m_ip(ip)
, m_s(-1)
, m_ssl(NULL)
, m_ssl_ctx(NULL)
, m_player_current_time(0.0)
, m_player_current_time_update(0)
{
	m_ssl_ctx = SSL_CTX_new(TLSv1_client_method());

	if (!connect())
		throw std::runtime_error("Could not connect");
}

ChromeCast::~ChromeCast()
{
	if (m_ssl)
		SSL_shutdown(m_ssl);
	disconnect();
	SSL_CTX_free(m_ssl_ctx);
}

// establish connection to the ChromeCast device, m_init is set to indicate that
// the protocol needs to be bootstrapped.
bool ChromeCast::connect()
{
	m_s = socket(PF_INET, SOCK_STREAM, 0);
	if (m_s == -1) {
		syslog(LOG_CRIT, "socket() failed: %m");
		return false;
	}
	struct sockaddr_in inaddr;
	memset(&inaddr, 0, sizeof inaddr);
	inaddr.sin_family = AF_INET;
	inaddr.sin_port = htons(8009);
	inaddr.sin_addr.s_addr = inet_addr(m_ip.c_str());
	if (::connect(m_s, (const struct sockaddr *)&inaddr,
				sizeof inaddr) != 0) {
		syslog(LOG_CRIT, "connect() failed: %m");
		close(m_s);
		m_s = -1;
		return false;
	}

	m_ssl = SSL_new(m_ssl_ctx);
	SSL_set_fd(m_ssl, m_s);
	SSL_set_mode(m_ssl, SSL_get_mode(m_ssl) | SSL_MODE_AUTO_RETRY);
	SSL_set_connect_state(m_ssl);
	if (SSL_connect(m_ssl) != 1) {
		syslog(LOG_CRIT, "SSL_connect() failed");
		SSL_free(m_ssl);
		m_ssl = NULL;
		close(m_s);
		m_s = -1;
		return false;
	}
	m_tread = std::thread(&ChromeCast::_read, this);
	m_init = false;
	return true;
}

bool ChromeCast::init()
{
	static std::mutex m_init_mutex;
	std::lock_guard<std::mutex> lock(m_init_mutex);

	if (m_init)
		return true;

	_release_waiters();
	m_destination_id = "receiver-0";
	m_session_id = "";

	Json::Value msg, response;

	bool retry = false;
	do {
		if (!m_ssl)
			if (!connect())
				return false;
		msg = Json::objectValue;
		msg["type"] = "CONNECT";
		msg["origin"] = Json::Value(Json::objectValue);
		send("urn:x-cast:com.google.cast.tp.connection", msg);
		if (!m_ssl) {
			if (retry)
				return false;
			retry = true;
			syslog(LOG_DEBUG, "Retrying connect");
			continue;
		}
		break;
	} while (1);

	msg = Json::objectValue;
	msg["type"] = "GET_STATUS";
	msg["requestId"] = _request_id();
	response = send("urn:x-cast:com.google.cast.receiver", msg);

	if (response.isMember("status") &&
			response["status"].isMember("applications") &&
			response["status"]["applications"].isValidIndex(0u) &&
			response["status"]["applications"][0u].isMember("appId") &&
			response["status"]["applications"][0u]["appId"].asString() == "CC1AD845") {
		syslog(LOG_DEBUG, "Default Media Receiver already running");
	} else {
		msg = Json::objectValue;
		msg["type"] = "LAUNCH";
		msg["requestId"] = _request_id();
		msg["appId"] = "CC1AD845";
		response = send("urn:x-cast:com.google.cast.receiver", msg);
	}

	if (response.isMember("status") &&
			response["status"].isMember("applications") &&
			response["status"]["applications"].isValidIndex(0u)) {
		Json::Value& application = response["status"]["applications"][0u];
		m_destination_id = application["transportId"].asString();
		m_session_id = application["sessionId"].asString();
	} else {
		syslog(LOG_CRIT, "transportId and sessionId not found");
		return false;
	}

	msg = Json::objectValue;
	msg["type"] = "CONNECT";
	msg["origin"] = Json::Value(Json::objectValue);
	send("urn:x-cast:com.google.cast.tp.connection", msg);

	m_init = true;
	return true;
}

// disconnect() is called on error, so the background thread should already
// have exiteded... and be joinable...
void ChromeCast::disconnect()
{
	m_tread.join();
	if (m_s != -1)
		close(m_s);
	m_s = -1;
	SSL_free(m_ssl);
	m_ssl = NULL;
	m_init = false;
}

Json::Value ChromeCast::send(const std::string& namespace_, const Json::Value& payload)
{
	Json::FastWriter fw;
	fw.omitEndingLineFeed();

	extensions::core_api::cast_channel::CastMessage msg;
	msg.set_payload_type(msg.STRING);
	msg.set_protocol_version(msg.CASTV2_1_0);
	msg.set_namespace_(namespace_);
	msg.set_source_id(m_source_id);
	msg.set_destination_id(m_destination_id);
	msg.set_payload_utf8(fw.write(payload));

	std::string data, foo;
	char pktlen[4];
	unsigned int len = htonl(msg.ByteSize());
	memcpy(&pktlen, &len, sizeof len);
	foo.append(pktlen, sizeof pktlen);
	msg.SerializeToString(&data);
	foo += data;

	std::condition_variable f;
	bool wait = false;
	unsigned int requestId;
	if (payload.isMember("requestId"))
	{
		requestId = payload["requestId"].asUInt();
		m_mutex.lock();
		m_wait[requestId] = std::make_pair(&f, std::string());
		wait = true;
		m_mutex.unlock();
	}

	syslog(LOG_DEBUG, "%s -> %s (%s): %s",
			msg.source_id().c_str(),
			msg.destination_id().c_str(),
			msg.namespace_().c_str(),
			msg.payload_utf8().c_str()
		  );

	int w;
	m_ssl_mutex.lock();
	if (m_ssl) {
		w = SSL_write(m_ssl, foo.c_str(), foo.size());
		if (w == -1) {
			syslog(LOG_DEBUG, "SSL_write error");
			disconnect();
		}
	} else
		w = -1;
	m_ssl_mutex.unlock();

	if (wait && w == -1)
	{
		m_mutex.lock();
		m_wait.erase(requestId);
		m_mutex.unlock();
		wait = false;
	}

	Json::Value ret;
	if (wait) {
		std::unique_lock<std::mutex> wl(m_mutex);
		f.wait(wl);
		ret = m_wait[requestId].second;
		m_wait.erase(requestId);
		m_mutex.unlock();
	}

	return ret;
}

void ChromeCast::_read()
{
	while (true)
	{
		char pktlen[4];
		int r;
		for (r = 0; r < sizeof pktlen; ++r)
			if (SSL_read(m_ssl, pktlen + r, 1) < 1)
				break;
		if (r != sizeof pktlen) {
			syslog(LOG_ERR, "SSL_read error");
			_release_waiters();
			return;
		}

		unsigned int len;
		memcpy(&len, pktlen, sizeof len);
		len = ntohl(len);

		std::string buf;
		while (buf.size() < len) {
			char b[2048];
			int r = SSL_read(m_ssl, b, sizeof b);
			if (r < 1)
				break;
			buf.append(b, r);
		}
		if (buf.size() != len) {
			syslog(LOG_ERR, "SSL_read error");
			_release_waiters();
			return;
		}

		extensions::core_api::cast_channel::CastMessage msg;
		msg.ParseFromString(buf);

		syslog(LOG_DEBUG, "%s -> %s (%s): %s",
				msg.source_id().c_str(),
				msg.destination_id().c_str(),
				msg.namespace_().c_str(),
				msg.payload_utf8().c_str()
			  );

		Json::Value response;
		Json::Reader reader;
		if (!reader.parse(msg.payload_utf8(), response, false))
			continue;

		if (msg.namespace_() == "urn:x-cast:com.google.cast.tp.heartbeat")
		{
			Json::FastWriter fw;
			fw.omitEndingLineFeed();

			extensions::core_api::cast_channel::CastMessage reply(msg);
			Json::Value msg;
			msg["type"] = "PONG";
			reply.set_payload_utf8(fw.write(msg));
			std::string data;
			reply.SerializeToString(&data);
			unsigned int len = htonl(data.size());
			SSL_write(m_ssl, &len, sizeof len);
			SSL_write(m_ssl, data.c_str(), data.size());
			continue;
		}

		if (msg.namespace_() == "urn:x-cast:com.google.cast.tp.connection")
		{
			if (response["type"].asString() == "CLOSE")
			{
				_release_waiters();
				m_init = false;
			}
		}

		if (msg.namespace_() == "urn:x-cast:com.google.cast.media")
		{
			if (response["type"].asString() == "MEDIA_STATUS")
			{
				if (response.isMember("status") &&
					response["status"].isValidIndex(0u))
				{
					Json::Value& status = response["status"][0u];
					m_media_session_id = status["mediaSessionId"].asUInt();
				}
			}
		}

		if (response.isMember("requestId"))
		{
			m_mutex.lock();
			auto waitIter = m_wait.find(response["requestId"].asUInt());
			if (waitIter != m_wait.end())
			{
				waitIter->second.second = response;
				waitIter->second.first->notify_all();
			}
			m_mutex.unlock();
		}

		if (msg.namespace_() == "urn:x-cast:com.google.cast.media")
		{
			if (response["type"].asString() == "MEDIA_STATUS")
			{
				if (response.isMember("status") &&
					response["status"].isValidIndex(0u))
				{
					std::string uuid = m_uuid;
					Json::Value& status = response["status"][0u];
					if (status.isMember("activeTrackIds"))
						m_subtitles = status["activeTrackIds"].isValidIndex(0u);
					m_player_state = status["playerState"].asString();
					m_player_current_time = status["currentTime"].asDouble();
					m_player_current_time_update = time(NULL);
					if (status["playerState"] == "IDLE")
						m_uuid = "";
					if (status["playerState"] != "IDLE" &&
							!status["media"]["customData"]["uuid"].asString().empty())
						uuid = m_uuid = status["media"]["customData"]["uuid"].asString();
					if (m_mediaStatusCallback)
						m_mediaStatusCallback(status["playerState"].asString(),
								status["idleReason"].asString(),
								uuid);
				}
			}
		}
	}
}

const std::string& ChromeCast::getUUID() const
{
	return m_uuid;
}

const std::string& ChromeCast::getPlayerState() const
{
	return m_player_state;
}

double ChromeCast::getPlayerCurrentTime() const
{
	if (m_player_current_time == 0)
		return 0;
	return m_player_current_time + (time(NULL) - m_player_current_time_update);
}

bool ChromeCast::hasSubtitles() const
{
	return m_subtitles;
}

std::string ChromeCast::getSocketName() const
{
	struct sockaddr_in addr;
    socklen_t len = sizeof(struct sockaddr);
	getsockname(m_s, (struct sockaddr*)&addr, &len);
	return inet_ntoa(addr.sin_addr);
}

bool isPlayerState(const Json::Value& response, const std::string& playerState)
{
	if (response.isMember("type") && response["type"].asString() == "MEDIA_STATUS")
	{
		if (response.isMember("status") &&
				response["status"].isValidIndex(0u))
		{
			const Json::Value& status = response["status"][0u];
			if (status.isMember("playerState") && status["playerState"].asString() == playerState)
				return true;
		}
	}
	return false;
}

bool ChromeCast::load(const std::string& url, const std::string& title, const std::string& uuid)
{
	if (!m_init && !init())
		return false;
	Json::Value msg, response;
	msg["type"] = "LOAD";
	msg["requestId"] = _request_id();
	msg["sessionId"] = m_session_id;
	msg["media"]["contentId"] = url;
	msg["media"]["streamType"] = "buffered";
	msg["media"]["contentType"] = "video/x-matroska";
	msg["media"]["customData"]["uuid"] = uuid;
	msg["media"]["metadata"]["title"] = title;
	msg["media"]["tracks"] = Json::arrayValue;
	msg["media"]["tracks"][0] = Json::objectValue;
	msg["media"]["tracks"][0]["language"] = "en-US";
	msg["media"]["tracks"][0]["name"] = "English";
	msg["media"]["tracks"][0]["type"] = "TEXT";
	msg["media"]["tracks"][0]["subtype"] = "SUBTITLES";
	msg["media"]["tracks"][0]["trackId"] = 1;
	msg["media"]["tracks"][0]["trackContentId"] = "trk0002";
	msg["media"]["tracks"][0]["trackContentId"] = std::string(url).replace(std::string(url).find("stream/"), 6, "subs");
	msg["media"]["tracks"][0]["trackContentType"] = "text/vtt";
	msg["media"]["textTrackStyle"]["backgroundColor"] = "#00000000";
	msg["media"]["textTrackStyle"]["edgeType"] = "OUTLINE";
	msg["media"]["textTrackStyle"]["edgeColor"] = "#000000FF";
	msg["media"]["textTrackStyle"]["fontScale"] = 1.1;
	if (m_subtitles) {
		msg["activeTrackIds"] = Json::arrayValue;
		msg["activeTrackIds"][0] = 1;
	}
	msg["autoplay"] = true;
	msg["currentTime"] = 0;
	response = send("urn:x-cast:com.google.cast.media", msg);
	return isPlayerState(response, "BUFFERING") || isPlayerState(response, "PLAYING");
}

bool ChromeCast::pause()
{
	if (!m_init && !init())
		return false;
	Json::Value msg, response;
	msg["type"] = "PAUSE";
	msg["requestId"] = _request_id();
	msg["mediaSessionId"] = m_media_session_id;
	response = send("urn:x-cast:com.google.cast.media", msg);
	return isPlayerState(response, "PAUSED");
}

bool ChromeCast::play()
{
	if (!m_init && !init())
		return false;
	Json::Value msg, response;
	msg["type"] = "PLAY";
	msg["requestId"] = _request_id();
	msg["mediaSessionId"] = m_media_session_id;
	response = send("urn:x-cast:com.google.cast.media", msg);
	return isPlayerState(response, "BUFFERING") || isPlayerState(response, "PLAYING");
}

bool ChromeCast::stop()
{
	if (!m_init && !init())
		return false;
	Json::Value msg, response;
	msg["type"] = "STOP";
	msg["requestId"] = _request_id();
	msg["mediaSessionId"] = m_media_session_id;
	response = send("urn:x-cast:com.google.cast.media", msg);
	return isPlayerState(response, "IDLE");
}

bool ChromeCast::setSubtitles(bool status)
{
	if (!m_init && !init())
		return false;
	Json::Value msg, response;
	msg["type"] = "EDIT_TRACKS_INFO";
	msg["requestId"] = _request_id();
	msg["mediaSessionId"] = m_media_session_id;
	msg["activeTrackIds"] = Json::arrayValue;
	if (status)
		msg["activeTrackIds"][0] = 1;
	response = send("urn:x-cast:com.google.cast.media", msg);
	return true;
}

void ChromeCast::setSubtitleSettings(bool status)
{
	m_subtitles = status;
}

void ChromeCast::setMediaStatusCallback(std::function<void(const std::string&,
			const std::string&, const std::string&)> func)
{
	m_mediaStatusCallback = func;
}

unsigned int ChromeCast::_request_id()
{
	unsigned int request_id;
	m_mutex.lock();
	request_id = m_request_id++;
	m_mutex.unlock();
	return request_id;
}

void ChromeCast::_release_waiters()
{
	m_mutex.lock();
	for (auto& item : m_wait) {
		item.second.second = Json::Value();
		item.second.first->notify_all();
	}
	m_mutex.unlock();
}
