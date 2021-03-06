#ifndef __OSMOSIS_CLIENT_TRANSFER_H__
#define __OSMOSIS_CLIENT_TRANSFER_H__

#include "Osmosis/Client/TransferThread.h"
#include "Osmosis/Client/CheckExistingThread.h"

namespace Osmosis {
namespace Client
{

class Transfer
{
public:
	Transfer(       const std::string &            label,
			Chain::Chain &                 chain,
			Chain::ObjectStoreInterface &  destination ) :
		_label( label ),
		_chain( chain ),
		_destination( destination ),
		_toCheckExistingQueue( 1 ),
		_toTransferQueue( CHECK_EXISTING_THREADS )
	{
		for ( unsigned i = 0; i < CHECK_EXISTING_THREADS; ++ i )
			_threads.push_back( std::thread(
				CheckExistingThread::task, std::ref( _toCheckExistingQueue ), std::ref( _toTransferQueue ), std::ref( destination ),
				std::ref( _checkExistingAlreadyProcessed ), std::ref( _checkExistingAlreadyProcessedLock ) ) );
		for ( unsigned i = 0; i < TRANSFER_THREADS; ++ i )
			_threads.push_back( std::thread(
				TransferThread::task, std::ref( _toTransferQueue ), std::ref( chain ), std::ref( destination ) ) );
	}

	~Transfer()
	{
		_toCheckExistingQueue.abort();
		_toTransferQueue.abort();
		for ( auto & i : _threads )
			if ( i.joinable() )
				i.join();
	}

	void go()
	{
		BACKTRACE_BEGIN
		auto connection = _destination.connect();
		checkLabelDoesNotExistInDestination( * connection );

		Chain::CheckOut checkOut( _chain.checkOut() );
		Hash labelHash = checkOut.getLabel( _label );
		DirList labelDirList = fetchDirList( labelHash, checkOut );

		populateToCheckExistingQueue( labelHash, labelDirList );
		for ( auto & i : _threads )
			i.join();
		connection->setLabel( labelHash, _label );
		BACKTRACE_END
	}

private:
	enum {
		CHECK_EXISTING_THREADS = 10,
		TRANSFER_THREADS = 5,
	};

	const std::string                      _label;
	Chain::Chain &                         _chain;
	Chain::ObjectStoreInterface &          _destination;
	CheckExistingThread::AlreadyProcessed  _checkExistingAlreadyProcessed;
	std::mutex                             _checkExistingAlreadyProcessedLock;
	DigestedTaskQueue                      _toCheckExistingQueue;
	DigestedTaskQueue                      _toTransferQueue;
	std::vector< std::thread >             _threads;

	void checkLabelDoesNotExistInDestination( Chain::ObjectStoreConnectionInterface & connection )
	{
		if ( connection.listLabels( "^" + _label + "$" ).size() > 0 )
			THROW( Error, "Label '" << _label << "' already exists at destination" );
	}

	void populateToCheckExistingQueue( const Hash & labelHash, const DirList & labelDirList )
	{
		BACKTRACE_BEGIN
		for ( auto & entry : labelDirList.entries() )
			if ( entry.hash )
				_toCheckExistingQueue.put( Digested( { entry.path, * entry.hash } ) );
		_toCheckExistingQueue.put( Digested( { boost::filesystem::path( "label" ), labelHash } ) );
		_toCheckExistingQueue.producerDone();
		BACKTRACE_END
	}

	DirList fetchDirList( const Hash & labelHash, Chain::CheckOut & checkOut )
	{
		BACKTRACE_BEGIN
		std::string dirListText = checkOut.getString( labelHash );
		if ( not CalculateHash::verify( dirListText.c_str(), dirListText.size(), labelHash ) )
			THROW( Error, "Dir list hash did not match contents" );
		std::istringstream dirListTextStream( std::move( dirListText ) );
		DirList labelDirList;
		dirListTextStream >> labelDirList;
		return std::move( labelDirList );
		BACKTRACE_END
	}

	Transfer( const Transfer & rhs ) = delete;
	Transfer & operator= ( const Transfer & rhs ) = delete;
};

} // namespace Client
} // namespace Osmosis

#endif // __OSMOSIS_CLIENT_TRANSFER_H__
