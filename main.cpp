#include "webserver.h"

void addsig(int sig, void( handler )(int)){
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = handler;
    sigfillset( &sa.sa_mask );
    assert( sigaction( sig, &sa, NULL ) != -1 );
}

int main(int argc, char* argv[]){
    if( argc <= 3 ) {
        printf( "usage: %s port_number trigMode IOHandlePattern\n", basename(argv[0]));
        return 1;
    }

    int port = atoi( argv[1] );
    addsig( SIGPIPE, SIG_IGN );

    int listenTrig;
    int connTrig;
    int trigMode = atoi( argv[2] );
    if(trigMode == 0){ // LT + LT
        listenTrig = 0;
        connTrig = 0;
    }else if( trigMode == 1){ // LT + ET
        listenTrig = 0;
        connTrig = 1;
    }else if( trigMode == 2 ){ // ET + LT 
        listenTrig = 1;
        connTrig = 0;
    }else{ // ET + ET
        listenTrig = 1;
        connTrig = 1;
    }

    int pattern = atoi( argv[3] );

    WebServer * server = new WebServer(port, listenTrig, connTrig, pattern);

    server->start();

    return 0;
}