/**********************************
 * FILE NAME: Application.cpp
 *
 * DESCRIPTION: Application layer class function definitions
 **********************************/

#include "Application.h"

void handler(int sig) {
	void *array[10];
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(array, 10);

	// print out all the frames to stderr
	fprintf(stderr, "Error: signal %d:\n", sig);
	backtrace_symbols_fd(array, size, STDERR_FILENO);
	exit(1);
}

/**********************************
 * FUNCTION NAME: main
 *
 * DESCRIPTION: main function. Start from here
 **********************************/
int main(int argc, char *argv[]) {
	//signal(SIGSEGV, handler);
	if ( argc != ARGS_COUNT ) {
		cout<<"Configuration (i.e., *.conf) file File Required"<<endl;
		return FAILURE;
	}

	// Create a new application object
	Application *app = new Application(argv[1]);
	// Call the run function
	app->run();
	// When done delete the application object
	delete(app);

	return SUCCESS;
}

/**
 * Constructor of the Application class
 */
Application::Application(char *infile) {
	int i;
	par = new Params();
	srand (time(NULL));
	par->setparams(infile);
	log = new Log(par);
	en = new EmulNet(par);
	mp1 = (MP1Node **) malloc(par->EN_GPSZ * sizeof(MP1Node *));

	/*
	 * Init all nodes
	 */
	for( i = 0; i < par->EN_GPSZ; i++ ) {
		Member *memberNode = new Member;
		memberNode->inited = false;
		Address *addressOfMemberNode = new Address();
		Address joinaddr;
		joinaddr = getjoinaddr();
		addressOfMemberNode = (Address *) en->ENinit(addressOfMemberNode, par->PORTNUM);
		mp1[i] = new MP1Node(memberNode, par, en, log, addressOfMemberNode);
		log->LOG(&(mp1[i]->getMemberNode()->addr), "APP");
		delete addressOfMemberNode;
	}
}

/**
 * Destructor
 */
Application::~Application() {
	delete log;
	delete en;
	for ( int i = 0; i < par->EN_GPSZ; i++ ) {
		delete mp1[i];
	}
	free(mp1);
	delete par;
}

/**
 * FUNCTION NAME: run
 *
 * DESCRIPTION: Main driver function of the Application layer
 */
int Application::run()
{
	int i;
	int timeWhenAllNodesHaveJoined = 0;
	// boolean indicating if all nodes have joined
	bool allNodesJoined = false;
	srand(time(NULL));

	// As time runs along
	for( par->globaltime = 0; par->globaltime < TOTAL_RUNNING_TIME; ++par->globaltime ) {
		// Run the membership protocol
		mp1Run();
		// Fail some nodes
		fail();
	}

	// Clean up
	en->ENcleanup();

	for(i=0;i<=par->EN_GPSZ-1;i++) {
		 mp1[i]->finishUpThisNode();
	}

	return SUCCESS;
}

/**
 * FUNCTION NAME: mp1Run
 *
 * DESCRIPTION:	This function performs all the membership protocol functionalities
 */
void Application::mp1Run() {
	int i;

	// For all the nodes in the system
	for( i = 0; i <= par->EN_GPSZ-1; i++) {

		/*
		 * Receive messages from the network and queue them in the membership protocol queue
		 */
		if( par->getcurrtime() > (int)(par->STEP_RATE*i) && !(mp1[i]->getMemberNode()->bFailed) ) {
			// Receive messages from the network and queue them
			mp1[i]->recvLoop();
		}

	}

	// For all the nodes in the system
	for( i = par->EN_GPSZ - 1; i >= 0; i-- ) {

		/*
		 * Introduce nodes into the distributed system
		 */
		if( par->getcurrtime() == (int)(par->STEP_RATE*i) ) {
			// introduce the ith node into the system at time STEPRATE*i
			mp1[i]->nodeStart(JOINADDR, par->PORTNUM);
			cout<<i<<"-th introduced node is assigned with the address: "<<mp1[i]->getMemberNode()->addr.getAddress() << endl;
			nodeCount += i;
		}

		/*
		 * Handle all the messages in your queue and send heartbeats
		 */
		else if( par->getcurrtime() > (int)(par->STEP_RATE*i) && !(mp1[i]->getMemberNode()->bFailed) ) {
			// handle messages and send heartbeats
			mp1[i]->nodeLoop();
			#ifdef DEBUGLOG
			if( (i == 0) && (par->globaltime % 500 == 0) ) {
				log->LOG(&mp1[i]->getMemberNode()->addr, "@@time=%d", par->getcurrtime());
			}
			#endif
		}

	}
}

/**
 * FUNCTION NAME: fail
 *
 * DESCRIPTION: This function controls the failure of nodes
 *
 * Note: this is used only by MP1
 */
void Application::fail() {
	int i, removed;

	// fail half the members at time t=400
	if( par->DROP_MSG && par->getcurrtime() == 50 ) {
		par->dropmsg = 1;
	}

	if( par->SINGLE_FAILURE && par->getcurrtime() == 100 ) {
		removed = (rand() % par->EN_GPSZ);
		#ifdef DEBUGLOG
		log->LOG(&mp1[removed]->getMemberNode()->addr, "Node failed at time=%d", par->getcurrtime());
		#endif
		mp1[removed]->getMemberNode()->bFailed = true;
	}
	else if( par->getcurrtime() == 100 ) {
		removed = rand() % par->EN_GPSZ/2;
		for ( i = removed; i < removed + par->EN_GPSZ/2; i++ ) {
			#ifdef DEBUGLOG
			log->LOG(&mp1[i]->getMemberNode()->addr, "Node failed at time = %d", par->getcurrtime());
			#endif
			mp1[i]->getMemberNode()->bFailed = true;
		}
	}

	if( par->DROP_MSG && par->getcurrtime() == 300) {
		par->dropmsg=0;
	}

}

/**
 * FUNCTION NAME: getjoinaddr
 *
 * DESCRIPTION: This function returns the address of the coordinator
 */
Address Application::getjoinaddr(void){
	//trace.funcEntry("Application::getjoinaddr");
    Address joinaddr;
    joinaddr.init();
    *(int *)(&(joinaddr.addr))=1;
    *(short *)(&(joinaddr.addr[4]))=0;
    //trace.funcExit("Application::getjoinaddr", SUCCESS);
    return joinaddr;
}
int Application::findARandomNodeThatIsAlive() {
	int number;
	do {
		number = (rand()%par->EN_GPSZ);
	}while (mp2[number]->getMemberNode()->bFailed);
	return number;
}

void Application::initTestKVPairs() {
	srand(time(NULL));
	int i;
	string key;
	key.clear();
	testKVPairs.clear();
	int alphanumLen = sizeof(alphanum) - 1;
	while ( testKVPairs.size() != NUMBER_OF_INSERTS ) {
		for ( i = 0; i < KEY_LENGTH; i++ ) {
			key.push_back(alphanum[rand()%alphanumLen]);
		}
		string value = "value" + to_string(rand()%NUMBER_OF_INSERTS);
		testKVPairs[key] = value;
		key.clear();
	}
}
void Application::insertTestKVPairs() {
	int number = 0;

	/*
	 * Init a few test key value pairs
	 */
	initTestKVPairs();

	for ( map<string, string>::iterator it = testKVPairs.begin(); it != testKVPairs.end(); ++it ) {
		// Step 1. Find a node that is alive
		number = findARandomNodeThatIsAlive();

		// Step 2. Issue a create operation
		log->LOG(&mp2[number]->getMemberNode()->addr, "CREATE OPERATION KEY: %s VALUE: %s at time: %d", it->first.c_str(), it->second.c_str(), par->getcurrtime());
		mp2[number]->clientCreate(it->first, it->second);
	}

	cout<<endl<<"Sent " <<testKVPairs.size() <<" create messages to the ring"<<endl;
}
void Application::deleteTest() {
	int number;
	
	cout<<endl<<"Deleting "<<testKVPairs.size()/2 <<" valid keys.... ... .. . ."<<endl;
	map<string, string>::iterator it = testKVPairs.begin();
	for ( int i = 0; i < testKVPairs.size()/2; i++ ) {
		it++;

		// Step 1.a. Find a node that is alive
		number = findARandomNodeThatIsAlive();

		// Step 1.b. Issue a delete operation
		log->LOG(&mp2[number]->getMemberNode()->addr, "DELETE OPERATION KEY: %s VALUE: %s at time: %d", it->first.c_str(), it->second.c_str(), par->getcurrtime());
		mp2[number]->clientDelete(it->first);
	}
	

void Application::readTest() {

	
	map<string, string>::iterator it = testKVPairs.begin();
	int number;
	vector<Node> replicas;
	int replicaIdToFail = TERTIARY;
	int nodeToFail;
	bool failedOneNode = false;

	
	if ( par->getcurrtime() == TEST_TIME ) {
		// Step 1.a. Find a node that is alive
		number = findARandomNodeThatIsAlive();

		// Step 1.b Do a read operation
		cout<<endl<<"Reading a valid key.... ... .. . ."<<endl;
		log->LOG(&mp2[number]->getMemberNode()->addr, "READ OPERATION KEY: %s VALUE: %s at time: %d", it->first.c_str(), it->second.c_str(), par->getcurrtime());
		mp2[number]->clientRead(it->first);
	}
	if ( par->getcurrtime() == (TEST_TIME + FIRST_FAIL_TIME) ) {
		// Step 2.a Find a node that is alive and assign it as number
		number = findARandomNodeThatIsAlive();

		// Step 2.b Find the replicas of this key
		replicas.clear();
		replicas = mp2[number]->findNodes(it->first);
		// if less than quorum replicas are found then exit
		if ( replicas.size() < (RF-1) ) {
			cout<<endl<<"Could not find at least quorum replicas for this key. Exiting!!! size of replicas vector: "<<replicas.size()<<endl;
			log->LOG(&mp2[number]->getMemberNode()->addr, "Could not find at least quorum replicas for this key. Exiting!!! size of replicas vector: %d", replicas.size());
			exit(1);
		}

		// Step 2.c Fail a replica
		for ( int i = 0; i < par->EN_GPSZ; i++ ) {
			if ( mp2[i]->getMemberNode()->addr.getAddress() == replicas.at(replicaIdToFail).getAddress()->getAddress() ) {
				if ( !mp2[i]->getMemberNode()->bFailed ) {
					nodeToFail = i;
					failedOneNode = true;
					break;
				}
				else {
					// Since we fail at most two nodes, one of the replicas must be alive
					if ( replicaIdToFail > 0 ) {
						replicaIdToFail--;
					}
					else {
						failedOneNode = false;
					}
				}
			}
		}
		if ( failedOneNode ) {
			log->LOG(&mp2[nodeToFail]->getMemberNode()->addr, "Node failed at time=%d", par->getcurrtime());
			mp2[nodeToFail]->getMemberNode()->bFailed = true;
			mp1[nodeToFail]->getMemberNode()->bFailed = true;
			cout<<endl<<"Failed a replica node"<<endl;
		}
		else {
			// The code can never reach here
			log->LOG(&mp2[number]->getMemberNode()->addr, "Could not fail a node");
			cout<<"Could not fail a node. Exiting!!!";
			exit(1);
		}

		number = findARandomNodeThatIsAlive();

		// Step 2.d Issue a read
		cout<<endl<<"Reading a valid key.... ... .. . ."<<endl;
		log->LOG(&mp2[number]->getMemberNode()->addr, "READ OPERATION KEY: %s VALUE: %s at time: %d", it->first.c_str(), it->second.c_str(), par->getcurrtime());
		mp2[number]->clientRead(it->first);

		failedOneNode = false;
	}
	
	/** end of test 2 **/

