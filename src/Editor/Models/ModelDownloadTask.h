#pragma once

#include "ModelCatalog.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/// Stage of an install, for the progress readout.
enum class eModelDownloadPhase : int
{
	idle,
	checkingDiskSpace,
	fetchingManifest,
	downloading,
	verifying,  ///< hashing a file that was already on disk, or one just fetched
	assembling, ///< concatenating split parts back into the file a graph names
	complete,
	failed,
};

/// Downloads and installs one or more catalog models on a background thread.
///
/// Resumable at file granularity: a file already on disk with the right size
/// and hash is skipped, so a cancelled or failed 3.8GB install does not start
/// over from zero. Nothing is left that a presence check would accept unless
/// it is genuinely complete, because every transfer lands under a .part name
/// and is renamed only once it verifies.
class ModelDownloadTask
{
public:
	ModelDownloadTask()= default;
	~ModelDownloadTask();

	ModelDownloadTask(const ModelDownloadTask&)= delete;
	ModelDownloadTask& operator=(const ModelDownloadTask&)= delete;

	struct Status
	{
		eModelDownloadPhase phase= eModelDownloadPhase::idle;

		/// Which model is being installed, and the file within it. The id
		/// rather than a display name: this runs on a worker thread and each
		/// caller renders it its own way, localized in the UI and as the
		/// catalog's short name on the console.
		eModelId currentModel= eModelId::INVALID;
		std::string currentFile;

		uint64_t currentFileReceivedBytes= 0;
		uint64_t currentFileTotalBytes= 0;
		/// Across every file of every model in this run. The total is only
		/// known once each model's manifest has been read.
		uint64_t overallReceivedBytes= 0;
		uint64_t overallTotalBytes= 0;

		bool bFinished= false;
		bool bSucceeded= false;
		bool bCancelled= false;
		/// Untranslated detail for a failure: an HTTP status, a hash
		/// mismatch, a disk message. The UI pairs it with a localized line.
		std::string errorDetail;
	};

	/// Starts installing the given models in order. Ignored while a run is
	/// already in flight.
	void start(const std::vector<eModelId>& models);

	/// Signals the thread to stop and joins it. Safe to call when idle, and
	/// called by the destructor, so an app stage that owns a task only has to
	/// destroy it.
	void cancelAndJoin();

	bool isRunning() const { return m_bRunning.load(); }

	/// A consistent snapshot, safe to call from the UI thread every frame.
	Status getStatus() const;

private:
	void runThread(std::vector<eModelId> models);
	bool installModel(const ModelCatalogEntry& entry);

	/// One logical file of a model: either a whole file, or a file that
	/// arrives as numbered parts to be concatenated.
	struct PlannedFile
	{
		std::string name;
		std::string sha256;
		uint64_t sizeBytes= 0;
		/// Empty for a file that is fetched whole.
		std::vector<PlannedFile> parts;
	};

	bool planFromManifest(const ModelCatalogEntry& entry, std::vector<PlannedFile>& outFiles,
						  std::vector<std::string>& outSidecarFiles);
	bool planFromCatalog(const ModelCatalogEntry& entry, std::vector<PlannedFile>& outFiles);
	/// bRequired false means a failure is logged and the install carries on,
	/// for a document that rides along rather than something a model needs.
	bool fetchOne(const std::string& url, const std::filesystem::path& destPath, const PlannedFile& file,
				  bool bRequired= true);
	bool assembleParts(const PlannedFile& file, const std::filesystem::path& directory);

	bool isCancelled() const { return m_bCancelRequested.load(); }
	void setPhase(eModelDownloadPhase phase);
	void fail(const std::string& detail);

	mutable std::mutex m_statusMutex;
	Status m_status;

	std::thread m_thread;
	std::atomic<bool> m_bRunning{false};
	std::atomic<bool> m_bCancelRequested{false};
};
