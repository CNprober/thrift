#include "Twitter.h"
#include <limits>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TSocket.h>

#include <boost/shared_ptr.hpp>

using namespace thrift;
using namespace thrift::example;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::protocol;

using boost::shared_ptr;

int main(int argc, char* argv[])
{
	shared_ptr<TTransport> socket (new TSocket("localhost", 9090));
	shared_ptr<TTransport> transport (new TBufferedTransport(socket));
	shared_ptr<TProtocol> protocol (new TBinaryProtocol(transport));

	TwitterClient client(protocol);

	transport->open();
	Tweet tweet;
	client.postTweet(tweet);
	transport->close();

	return 0;
}


