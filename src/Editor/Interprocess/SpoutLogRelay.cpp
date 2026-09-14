#include "SpoutLogRelay.h"
#include "Logger.h"
#include "PathUtils.h"
#include "SpoutLibrary.h"

#include <cstdlib>
#include <fstream>

// Sits next to MikanXR.log. Spout truncates it whenever logging is enabled, so each
// session starts clean and the relay only ever forwards lines from this run.
static const char* k_spoutLogFileName= "MikanSpout.log";

// Spout tags notice and above with a lowercase "[level] " prefix. Verbose lines, which
// are the bulk of the output, carry no tag at all.
static void emitSpoutLogLine(const std::string& line)
{
	std::string message= line;
	if (!message.empty() && message.back() == '\r')
		message.pop_back();
	if (message.empty())
		return;

	LogSeverityLevel severity= LogSeverityLevel::info;
	if (message[0] == '[')
	{
		const size_t tagEnd= message.find(']');
		if (tagEnd != std::string::npos)
		{
			const std::string tag= message.substr(1, tagEnd - 1);
			bool bIsLevelTag= true;

			if (tag == "warning")
				severity= LogSeverityLevel::warning;
			else if (tag == "error")
				severity= LogSeverityLevel::error;
			else if (tag == "fatal")
				severity= LogSeverityLevel::fatal;
			else if (tag == "notice")
				severity= LogSeverityLevel::info;
			else
				bIsLevelTag= false;

			// Only a recognized level is a prefix worth stripping: a verbose line is
			// free to start with a bracket of its own. Spout writes exactly one space
			// after the tag, so any further indentation belongs to the message and
			// marks it as detail under the line above.
			if (bIsLevelTag)
			{
				size_t messageStart= tagEnd + 1;
				if (messageStart < message.size() && message[messageStart] == ' ')
					messageStart++;

				message= message.substr(messageStart);
				if (message.empty())
					return;
			}
		}
	}

	switch (severity)
	{
	case LogSeverityLevel::warning:
		MIKAN_LOG_WARNING("Spout") << message;
		break;
	case LogSeverityLevel::error:
		MIKAN_LOG_ERROR("Spout") << message;
		break;
	case LogSeverityLevel::fatal:
		MIKAN_LOG_FATAL("Spout") << message;
		break;
	default:
		MIKAN_LOG_INFO("Spout") << message;
		break;
	}
}

SpoutLogRelay::SpoutLogRelay()
	: m_logPath(PathUtils::getProjectsRootDirectory() / k_spoutLogFileName)
{
}

SpoutLogRelay::~SpoutLogRelay() { setEnabled(false); }

void SpoutLogRelay::setEnabled(bool bEnabled)
{
	if (bEnabled == m_bEnabled)
		return;

	if (bEnabled)
	{
		// MIKAN_SPOUT_LOG points Spout's logging somewhere else entirely (see
		// SharedTextureWriter.cpp), so the relay stays out of the way while it is set
		if (std::getenv("MIKAN_SPOUT_LOG") != nullptr)
		{
			MIKAN_LOG_INFO("SpoutLogRelay")
				<< "MIKAN_SPOUT_LOG is set, leaving Spout logging to the environment variable";
			return;
		}

		m_spoutLibrary= GetSpout();
		if (m_spoutLibrary == nullptr)
		{
			MIKAN_LOG_ERROR("SpoutLogRelay") << "Failed to open the Spout library to configure logging";
			return;
		}

		m_partialLine.clear();

		// File logging only: EnableSpoutLog() would allocate a console window, which is
		// the thing the relay exists to replace
		m_spoutLibrary->EnableSpoutLogFile(m_logPath.string().c_str());
		m_spoutLibrary->SetSpoutLogLevel(LibLogLevel::SPOUT_LOG_VERBOSE);

		// Start reading past the banner Spout writes when the file is opened
		std::error_code errorCode;
		const std::streamoff bannerSize= (std::streamoff)std::filesystem::file_size(m_logPath, errorCode);
		m_readOffset= errorCode ? 0 : bannerSize;

		m_bEnabled= true;

		MIKAN_LOG_INFO("SpoutLogRelay") << "Relaying Spout logs from " << m_logPath;
	}
	else
	{
		// Pick up anything Spout logged since the last tick before closing the file
		drain();

		if (m_spoutLibrary != nullptr)
		{
			m_spoutLibrary->DisableSpoutLog();
			m_spoutLibrary->Release();
			m_spoutLibrary= nullptr;
		}

		m_bEnabled= false;
	}
}

void SpoutLogRelay::update()
{
	if (m_bEnabled)
	{
		drain();
	}
}

void SpoutLogRelay::drain()
{
	if (!m_bEnabled)
		return;

	std::error_code errorCode;
	const std::streamoff fileSize= (std::streamoff)std::filesystem::file_size(m_logPath, errorCode);
	if (errorCode)
		return;

	// Spout truncates the file each time file logging is enabled
	if (fileSize < m_readOffset)
	{
		m_readOffset= 0;
		m_partialLine.clear();
	}

	if (fileSize == m_readOffset)
		return;

	// Spout opens, appends and closes the file per log line, so reading it here never
	// contends with a held handle
	std::ifstream logStream(m_logPath, std::ios::binary);
	if (!logStream.is_open())
		return;

	logStream.seekg(m_readOffset);

	std::string block((size_t)(fileSize - m_readOffset), '\0');
	logStream.read(&block[0], (std::streamsize)block.size());
	block.resize((size_t)logStream.gcount());
	if (block.empty())
		return;

	m_readOffset+= (std::streamoff)block.size();
	m_partialLine+= block;

	// Emit whole lines only and hold back any tail that has no newline yet
	size_t lineStart= 0;
	size_t newlinePos= m_partialLine.find('\n');
	while (newlinePos != std::string::npos)
	{
		emitSpoutLogLine(m_partialLine.substr(lineStart, newlinePos - lineStart));
		lineStart= newlinePos + 1;
		newlinePos= m_partialLine.find('\n', lineStart);
	}
	m_partialLine.erase(0, lineStart);
}
