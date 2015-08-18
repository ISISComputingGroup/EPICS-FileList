#ifndef WatchPoller_H
#define WatchPoller_H
#include <efsw.hpp>

class WatchPoller: public epicsThreadRunable {
public:
	WatchPoller(FileList *parent, epicsMessageQueue pollRequest);
	virtual void run();
	~WatchPoller();
private:
	FileList *parent_;
	efsw::FileWatcher * fileWatcher_;
	efsw::FileWatchListener * pListener_;
	efsw::WatchID watchID_;
	epicsMessageQueue pollRequest_;
}

#endif /* WatchPoller_H */