#include "HttpInterprocessMessageServer.h"
#include "Logger.h"

#include "IxWebSocket/IXHttpServer.h"
#include "IxWebSocket/IXConnectionState.h"
#include "IxWebSocket/IxNetSystem.h"

#include <algorithm>
#include <chrono>
#include <future>

namespace
{
constexpr int k_httpRequestTimeoutMs= 1000;
// ix::HttpServer's deadline for reading one whole request, request line through body. The
// library default of 30 s would cut a multi-hundred-megabyte upload short over Wi-Fi.
constexpr int k_httpRequestDeadlineSecs= 300;

std::string stripQueryString(const std::string& uri)
{
	const size_t queryPos= uri.find('?');
	return queryPos != std::string::npos ? uri.substr(0, queryPos) : uri;
}

int hexDigitValue(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return -1;
}

// Percent-decodes one query string token, treating '+' as a space per the
// application/x-www-form-urlencoded convention. A malformed escape is left verbatim.
std::string percentDecode(const std::string& text)
{
	std::string decoded;
	decoded.reserve(text.size());

	for (size_t i= 0; i < text.size(); ++i)
	{
		const char c= text[i];

		if (c == '+')
		{
			decoded.push_back(' ');
		}
		else if (c == '%' && i + 2 < text.size())
		{
			const int high= hexDigitValue(text[i + 1]);
			const int low= hexDigitValue(text[i + 2]);

			if (high >= 0 && low >= 0)
			{
				decoded.push_back(static_cast<char>((high << 4) | low));
				i+= 2;
			}
			else
			{
				decoded.push_back(c);
			}
		}
		else
		{
			decoded.push_back(c);
		}
	}

	return decoded;
}

// Splits a request URI's "?key=value&..." tail into decoded pairs. A key with no '=' maps to
// an empty value, an empty key is dropped, and a repeated key keeps the last occurrence.
std::map<std::string, std::string> parseQueryString(const std::string& uri)
{
	std::map<std::string, std::string> queryArgs;

	const size_t queryPos= uri.find('?');
	if (queryPos == std::string::npos)
	{
		return queryArgs;
	}

	size_t pairStart= queryPos + 1;
	while (pairStart <= uri.size())
	{
		const size_t pairEnd= std::min(uri.find('&', pairStart), uri.size());
		const std::string pair= uri.substr(pairStart, pairEnd - pairStart);
		pairStart= pairEnd + 1;

		if (pair.empty())
		{
			continue;
		}

		const size_t equalsPos= pair.find('=');
		const std::string key= percentDecode(pair.substr(0, equalsPos));
		if (key.empty())
		{
			continue;
		}

		queryArgs[key]= equalsPos != std::string::npos ? percentDecode(pair.substr(equalsPos + 1)) : std::string();
	}

	return queryArgs;
}

std::string httpStatusDescription(int statusCode)
{
	switch (statusCode)
	{
	case 200:
		return "OK";
	case 400:
		return "Bad Request";
	case 404:
		return "Not Found";
	case 405:
		return "Method Not Allowed";
	case 409:
		return "Conflict";
	case 422:
		return "Unprocessable Entity";
	case 500:
		return "Internal Server Error";
	case 503:
		return "Service Unavailable";
	case 504:
		return "Gateway Timeout";
	default:
		return "Internal Server Error";
	}
}
} // namespace

struct PendingHttpRequest
{
	HttpRouteRequest routeRequest;
	std::shared_ptr<std::promise<HttpRouteResponse>> responsePromise;
};

struct PendingMainThreadJob
{
	std::function<void()> job;
	std::shared_ptr<std::promise<void>> donePromise;
};

HttpInterprocessMessageServer::HttpInterprocessMessageServer()
	: m_server(nullptr)
{
}

HttpInterprocessMessageServer::~HttpInterprocessMessageServer() { dispose(); }

bool HttpInterprocessMessageServer::initialize(int port, bool bAllowRemote)
{
	if (!ix::initNetSystem())
	{
		MIKAN_LOG_WARNING("HttpInterprocessMessageServer::initialize()") << "Failed to initialize net system";
		return false;
	}

	HttpInterprocessMessageServer* ownerServer= this;
	m_bDisposing= false;

	const std::string host= bAllowRemote ? "0.0.0.0" : "127.0.0.1";
	m_server= std::make_shared<ix::HttpServer>(port, host, ix::SocketServer::kDefaultTcpBacklog,
											   ix::SocketServer::kDefaultMaxConnections,
											   ix::SocketServer::kDefaultAddressFamily, k_httpRequestDeadlineSecs);
	m_server->setOnConnectionCallback(
		[ownerServer](ix::HttpRequestPtr request,
					  std::shared_ptr<ix::ConnectionState> connectionState) -> ix::HttpResponsePtr
		{ return ownerServer->handleIncomingRequest(request); });

	std::pair<bool, std::string> result= m_server->listen();
	if (!result.first)
	{
		MIKAN_LOG_WARNING("HttpInterprocessMessageServer::initialize()") << "Listen error: " << result.second;
		return false;
	}

	m_server->start();
	MIKAN_LOG_INFO("HttpInterprocessMessageServer::initialize()")
		<< "Listening on " << host << ":" << port << (bAllowRemote ? " (all interfaces)" : " (loopback only)");
	return true;
}

void HttpInterprocessMessageServer::dispose()
{
	// Parked connection threads are released before stop() joins them. Dropping a
	// promise raises future_error on its waiter, which answers 503.
	m_bDisposing= true;
	{
		std::lock_guard<std::mutex> lock(m_pendingRequestsMutex);
		m_pendingRequests.clear();
		m_pendingJobs.clear();
	}

	if (m_server)
	{
		m_server->stop();
		m_server= nullptr;

		ix::uninitNetSystem();
	}
}

bool HttpInterprocessMessageServer::setRouteHandler(const std::string& path, HttpRouteHandler handler)
{
	std::lock_guard<std::mutex> lock(m_routeHandlersMutex);

	if (m_routeHandlers.find(path) != m_routeHandlers.end())
	{
		MIKAN_LOG_WARNING("HttpInterprocessMessageServer::setRouteHandler") << "Route already registered: " << path;
		return false;
	}

	m_routeHandlers[path]= handler;
	return true;
}

void HttpInterprocessMessageServer::removeRouteHandler(const std::string& path)
{
	std::lock_guard<std::mutex> lock(m_routeHandlersMutex);

	m_routeHandlers.erase(path);
}

bool HttpInterprocessMessageServer::setBackgroundRouteHandler(const std::string& path, HttpRouteHandler handler)
{
	std::lock_guard<std::mutex> lock(m_routeHandlersMutex);

	if (m_routeHandlers.find(path) != m_routeHandlers.end()
		|| m_backgroundRouteHandlers.find(path) != m_backgroundRouteHandlers.end())
	{
		MIKAN_LOG_WARNING("HttpInterprocessMessageServer::setBackgroundRouteHandler")
			<< "Route already registered: " << path;
		return false;
	}

	m_backgroundRouteHandlers[path]= handler;
	return true;
}

void HttpInterprocessMessageServer::removeBackgroundRouteHandler(const std::string& path)
{
	std::lock_guard<std::mutex> lock(m_routeHandlersMutex);

	m_backgroundRouteHandlers.erase(path);
}

bool HttpInterprocessMessageServer::runOnMainThread(std::function<void()> job, int timeoutMs)
{
	if (m_bDisposing)
		return false;

	auto pendingJob= std::make_shared<PendingMainThreadJob>();
	pendingJob->job= std::move(job);
	pendingJob->donePromise= std::make_shared<std::promise<void>>();
	std::future<void> future= pendingJob->donePromise->get_future();

	// The queue is the promise's only owner from here, so dispose() clearing the queue
	// breaks the promise and wakes this thread. The disposing check sits under the same
	// lock dispose() clears under, so no job lands after the clear.
	{
		std::lock_guard<std::mutex> lock(m_pendingRequestsMutex);
		if (m_bDisposing)
			return false;
		m_pendingJobs.push_back(std::move(pendingJob));
	}

	if (future.wait_for(std::chrono::milliseconds(timeoutMs)) != std::future_status::ready)
		return false;

	try
	{
		// A promise dropped by dispose() also reads as ready, and get() raises its broken_promise
		future.get();
		return true;
	}
	catch (const std::future_error&)
	{
		return false;
	}
}

std::vector<std::string> HttpInterprocessMessageServer::getRegisteredRoutePaths() const
{
	std::vector<std::string> paths;

	std::lock_guard<std::mutex> lock(m_routeHandlersMutex);
	paths.reserve(m_routeHandlers.size());
	for (const auto& entry : m_routeHandlers)
	{
		paths.push_back(entry.first);
	}

	return paths;
}

bool HttpInterprocessMessageServer::invokeRouteHandler(const HttpRouteRequest& request, HttpRouteResponse& outResponse)
{
	HttpRouteHandler handler;
	{
		std::lock_guard<std::mutex> lock(m_routeHandlersMutex);

		auto handler_it= m_routeHandlers.find(request.path);
		if (handler_it == m_routeHandlers.end())
		{
			return false;
		}

		handler= handler_it->second;
	}

	outResponse= handler(request);
	return true;
}

ix::HttpResponsePtr HttpInterprocessMessageServer::handleIncomingRequest(ix::HttpRequestPtr request)
{
	const std::string path= stripQueryString(request->uri);

	// A background route answers on this thread. The body is moved rather than copied:
	// the library holds an upload twice already while reading it.
	HttpRouteHandler backgroundHandler;
	{
		std::lock_guard<std::mutex> lock(m_routeHandlersMutex);

		auto handler_it= m_backgroundRouteHandlers.find(path);
		if (handler_it != m_backgroundRouteHandlers.end())
		{
			backgroundHandler= handler_it->second;
		}
	}
	if (backgroundHandler)
	{
		HttpRouteRequest routeRequest;
		routeRequest.method= request->method;
		routeRequest.path= path;
		routeRequest.queryArgs= parseQueryString(request->uri);
		routeRequest.body= std::move(request->body);

		const HttpRouteResponse routeResponse= backgroundHandler(routeRequest);

		ix::WebSocketHttpHeaders headers;
		headers["Content-Type"]= routeResponse.contentType;

		return std::make_shared<ix::HttpResponse>(routeResponse.statusCode,
												  httpStatusDescription(routeResponse.statusCode),
												  ix::HttpErrorCode::Ok, headers, routeResponse.body);
	}

	auto pendingRequest= std::make_shared<PendingHttpRequest>();
	pendingRequest->routeRequest.method= request->method;
	pendingRequest->routeRequest.path= path;
	pendingRequest->routeRequest.queryArgs= parseQueryString(request->uri);
	pendingRequest->routeRequest.body= std::move(request->body);
	pendingRequest->responsePromise= std::make_shared<std::promise<HttpRouteResponse>>();

	std::future<HttpRouteResponse> future= pendingRequest->responsePromise->get_future();
	const std::string pendingPath= pendingRequest->routeRequest.path;

	// The queue is the promise's only owner, so dispose() clearing it wakes this thread
	{
		std::lock_guard<std::mutex> lock(m_pendingRequestsMutex);
		if (m_bDisposing)
		{
			pendingRequest.reset();
		}
		else
		{
			m_pendingRequests.push_back(std::move(pendingRequest));
		}
	}

	HttpRouteResponse routeResponse;
	try
	{
		if (future.wait_for(std::chrono::milliseconds(k_httpRequestTimeoutMs)) == std::future_status::ready)
		{
			routeResponse= future.get();
		}
		else
		{
			// The main thread may still fulfill this promise later; that's harmless since
			// std::promise::set_value() doesn't require a waiter to still be listening.
			MIKAN_MT_LOG_WARNING("HttpInterprocessMessageServer::handleIncomingRequest")
				<< "Timed out waiting for main thread to process request: " << pendingPath;
			routeResponse.statusCode= 504;
			routeResponse.body= R"({"error":"Timed out waiting for request to be processed"})";
		}
	}
	catch (const std::future_error&)
	{
		// The promise was dropped (e.g. server shutting down) without being fulfilled.
		MIKAN_MT_LOG_WARNING("HttpInterprocessMessageServer::handleIncomingRequest")
			<< "Request abandoned before it could be processed: " << pendingPath;
		routeResponse.statusCode= 503;
		routeResponse.body= R"({"error":"Server shutting down"})";
	}

	ix::WebSocketHttpHeaders headers;
	headers["Content-Type"]= routeResponse.contentType;

	return std::make_shared<ix::HttpResponse>(routeResponse.statusCode, httpStatusDescription(routeResponse.statusCode),
											  ix::HttpErrorCode::Ok, headers, routeResponse.body);
}

void HttpInterprocessMessageServer::processRequests()
{
	std::vector<PendingHttpRequestPtr> pendingRequests;
	std::vector<PendingMainThreadJobPtr> pendingJobs;
	{
		std::lock_guard<std::mutex> lock(m_pendingRequestsMutex);
		pendingRequests.swap(m_pendingRequests);
		pendingJobs.swap(m_pendingJobs);
	}

	for (const PendingMainThreadJobPtr& pendingJob : pendingJobs)
	{
		pendingJob->job();
		pendingJob->donePromise->set_value();
	}

	for (const PendingHttpRequestPtr& pendingRequest : pendingRequests)
	{
		HttpRouteHandler handler;
		{
			std::lock_guard<std::mutex> lock(m_routeHandlersMutex);

			auto handler_it= m_routeHandlers.find(pendingRequest->routeRequest.path);
			if (handler_it != m_routeHandlers.end())
			{
				handler= handler_it->second;
			}
		}

		HttpRouteResponse response;
		if (handler)
		{
			response= handler(pendingRequest->routeRequest);
		}
		else
		{
			MIKAN_LOG_WARNING("HttpInterprocessMessageServer::processRequests")
				<< "No route registered for path: " << pendingRequest->routeRequest.path;
			response.statusCode= 404;
			response.body= R"({"error":"No route registered for path"})";
		}

		pendingRequest->responsePromise->set_value(response);
	}
}
