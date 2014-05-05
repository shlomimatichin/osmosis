#ifndef __OSMOSIS_CLIENT_PUT_THREAD_H__
#define __OSMOSIS_CLIENT_PUT_THREAD_H__

namespace Osmosis {
namespace Client
{

class PutThread
{
public:
	static void task(       DigestedTaskQueue &  inputQueue,
				Connect &            connection )
	{
		try {
			PutThread( inputQueue, connection ).go();
		} CATCH_ALL( "Put thread terminated" )
	}

private:
	PutThread(      DigestedTaskQueue &  inputQueue,
			Connect &            connection ):
		_inputQueue( inputQueue ),
		_connect( connection )
	{}

	void work()
	{
		Digested task = _inputQueue.get();
		struct Tongue::Header header = { static_cast< unsigned char >( Tongue::Opcode::PUT ) };
		_connect.socket().sendAllConcated( header, task.hash.raw() );
		Stream::FileToSocket transfer( task.path.string().c_str(), _connect.socket() );
		transfer.transfer();
		TRACE_DEBUG( "Transferred file " << task.path );
	}

	void go()
	{
		try {
			while ( true )
				work();
		} catch ( DigestedTaskQueue::NoMoreTasksError ) {
			TRACE_DEBUG( "PutThread done" );
		}
	}

	DigestedTaskQueue &  _inputQueue;
	Connect &            _connect; 

	PutThread( const PutThread & rhs ) = delete;
	PutThread & operator= ( const PutThread & rhs ) = delete;
};

} // namespace Client
} // namespace Osmosis

#endif // __OSMOSIS_CLIENT_PUT_THREAD_H__
