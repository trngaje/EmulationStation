#pragma once
#ifndef ES_APP_SCRAPERS_SCRAPER_H
#define ES_APP_SCRAPERS_SCRAPER_H

#include "AsyncHandle.h"
#include "HttpReq.h"
#include "MetaData.h"
#include <functional>
#include <memory>
#include <queue>
#include <utility>
#include <assert.h>

#define MAX_SCRAPER_RESULTS 7

class FileData;
class SystemData;

struct ScraperSearchParams
{
	ScraperSearchParams() { overWriteMedias = true; }

	SystemData* system;
	FileData* game;

	bool overWriteMedias;
	std::string nameOverride;
};

struct ScraperSearchResult
{
	ScraperSearchResult() : mdl(GAME_METADATA) {};

	MetaDataList mdl;
	std::string imageUrl;
	std::string thumbnailUrl;
	std::string videoUrl;
	std::string marqueeUrl;

	bool hadMedia()
	{
		return !imageUrl.empty() || !thumbnailUrl.empty() || !videoUrl.empty() || !marqueeUrl.empty();
	}

	// Needed to pre-set the image type
	std::string imageType;
};

// So let me explain why I've abstracted this so heavily.
// There are two ways I can think of that you'd want to write a scraper.

// 1. Do some HTTP request(s) -> process it -> return the results
// 2. Do some local filesystem queries (an offline scraper) -> return the results

// The first way needs to be asynchronous while it's waiting for the HTTP request to return.
// The second doesn't.

// It would be nice if we could write it like this:
// search = generate_http_request(searchparams);
// wait_until_done(search);
// ... process search ...
// return results;

// We could do this if we used threads.  Right now ES doesn't because I'm pretty sure I'll fuck it up,
// and I'm not sure of the performance of threads on the Pi (single-core ARM).
// We could also do this if we used coroutines.  
// I can't find a really good cross-platform coroutine library (x86/64/ARM Linux + Windows), 
// and I don't want to spend more time chasing libraries than just writing it the long way once.

// So, I did it the "long" way.
// ScraperSearchHandle - one logical search, e.g. "search for mario"
// ScraperRequest - encapsulates some sort of asynchronous request that will ultimately return some results
// ScraperHttpRequest - implementation of ScraperRequest that waits on an HttpReq, then processes it with some processing function.


// a scraper search gathers results from (potentially multiple) ScraperRequests
class ScraperRequest : public AsyncHandle
{
public:
	ScraperRequest(std::vector<ScraperSearchResult>& resultsWrite);

	// returns "true" once we're done
	virtual void update() = 0;
	
protected:
	std::vector<ScraperSearchResult>& mResults;
};


// a single HTTP request that needs to be processed to get the results
class ScraperHttpRequest : public ScraperRequest
{
public:
	ScraperHttpRequest(std::vector<ScraperSearchResult>& resultsWrite, const std::string& url);
	virtual void update() override;

protected:
	virtual void process(const std::unique_ptr<HttpReq>& req, std::vector<ScraperSearchResult>& results) = 0;

private:
	std::unique_ptr<HttpReq> mReq;
};

// a request to get a list of results
class ScraperSearchHandle : public AsyncHandle
{
public:
	ScraperSearchHandle();

	void update();
	inline const std::vector<ScraperSearchResult>& getResults() const { assert(mStatus != ASYNC_IN_PROGRESS); return mResults; }

protected:
	friend std::unique_ptr<ScraperSearchHandle> startScraperSearch(const ScraperSearchParams& params);

	std::queue< std::unique_ptr<ScraperRequest> > mRequestQueue;
	std::vector<ScraperSearchResult> mResults;
};

// will use the current scraper settings to pick the result source
std::unique_ptr<ScraperSearchHandle> startScraperSearch(const ScraperSearchParams& params);

// returns a list of valid scraper names
std::vector<std::string> getScraperList();

// returns true if the scraper configured in the settings is still valid
bool isValidConfiguredScraper();

typedef void (*generate_scraper_requests_func)(const ScraperSearchParams& params, std::queue< std::unique_ptr<ScraperRequest> >& requests, std::vector<ScraperSearchResult>& results);

// -------------------------------------------------------------------------



// Meta data asset downloading stuff.
class MDResolveHandle : public AsyncHandle
{
public:
	MDResolveHandle(const ScraperSearchResult& result, const ScraperSearchParams& search);

	void update() override;
	inline const ScraperSearchResult& getResult() const { return mResult; } //  assert(mStatus == ASYNC_DONE); -> FCA : Why ???

	std::string getCurrentItem() {
		return mCurrentItem;
	}

	std::string getCurrentSource() {
		return mSource;
	}

	int getPercent() {
		return mPercent;
	}

private:
	ScraperSearchResult mResult;

	class ResolvePair
	{	
	public:
		ResolvePair(std::function<std::unique_ptr<AsyncHandle>()> _invoker, std::function<void()> _function, std::string _name, std::string _source)
		{
			func = _invoker;
			onFinished = _function;
			name = _name;
			source = _source;
		}

		void Run()
		{
			handle = func();
		}
	
		std::function<void()> onFinished;
		std::string name;
		std::string source;

		std::unique_ptr<AsyncHandle> handle;

	private:
		std::function<std::unique_ptr<AsyncHandle>()> func;
	};

	std::vector<ResolvePair*> mFuncs;
	std::string mCurrentItem;
	std::string mSource;
	int mPercent;
};

class ImageDownloadHandle : public AsyncHandle
{
public:
	ImageDownloadHandle(const std::string& url, const std::string& path, int maxWidth, int maxHeight);

	void update() override;

	virtual int getPercent();

private:
	std::unique_ptr<HttpReq> mReq;
	std::string mSavePath;
	int mMaxWidth;
	int mMaxHeight;
};

//About the same as "~/.emulationstation/downloaded_images/[system_name]/[game_name].[url's extension]".
//Will create the "downloaded_images" and "subdirectory" directories if they do not exist.
std::string getSaveAsPath(const ScraperSearchParams& params, const std::string& suffix, const std::string& url);

//Will resize according to Settings::getInt("ScraperResizeWidth") and Settings::getInt("ScraperResizeHeight").
std::unique_ptr<ImageDownloadHandle> downloadImageAsync(const std::string& url, const std::string& saveAs);

// Resolves all metadata assets that need to be downloaded.
std::unique_ptr<MDResolveHandle> resolveMetaDataAssets(const ScraperSearchResult& result, const ScraperSearchParams& search);

//You can pass 0 for maxWidth or maxHeight to automatically keep the aspect ratio.
//Will overwrite the image at [path] with the new resized one.
//Returns true if successful, false otherwise.
bool resizeImage(const std::string& path, int maxWidth, int maxHeight);

#endif // ES_APP_SCRAPERS_SCRAPER_H
