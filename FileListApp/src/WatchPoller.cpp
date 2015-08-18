#include "FileList.h"
#include <iostrema>
#include <string.h>


/// Processes a file action
class UpdateListener : public efsw::FileWatchListener
{
	private:
		FileList * parent;

	public:
		UpdateListener() : parent(NULL) {}

		void setParent (FileList * par ) {parent = par;}

		void handleFileAction( efsw::WatchID watchid, const std::string& dir, const std::string& filename, efsw::Action action, std::string oldFilename = ""  )
		{
			parent->updateList();
			std::cerr << "Updated! Watch:" << watchid << std::endl; 
		}
};

WatchPoller::WatchPoller(FileList *parent, epicsMessageQueue pollRequest)
{
	//Set message queue
	pollRequest_ = pollRequest;
}

void WatchPoller::run(void)
{
	char *directory; 			//Where to look

	//Create watcher
	fileWatcher_ = new efsw::FileWatcher ();

	//Create listener
	UpdateListener * listener = new UpdateListener ();
	listener->setParent(parent);
	pListener_ = listener;

	while(true)
	{
		pollRequest_.receive(&directory, sizeof(*char));

		std::cerr << "Request: " << directory << std::endl;
	}
}