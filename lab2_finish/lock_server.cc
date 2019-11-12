// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  pthread_mutex_init(&lock, NULL);
  locks = new std::map<lock_protocol::lockid_t, int>;
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
  pthread_mutex_lock(&lock);
  std::map<lock_protocol::lockid_t, int>::iterator iter = locks->find(lid);
  if(iter == locks->end()){
    printf("%d acquire %d new.\n", clt, (int)lid);
    locks->insert(std::pair<lock_protocol::lockid_t, int>(lid, 1));
  }else
  {
    while(iter->second != 0){
    }
    iter->second = 1;
    printf("%d acquire %d existed.\n", clt, (int)lid);
  }
  pthread_mutex_unlock(&lock);
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
  std::map<lock_protocol::lockid_t, int>::iterator iter = locks->find(lid);
  if(iter != locks->end() && iter->second == 1){
    printf("%d release %d ok.\n", clt, (int)lid);
    iter->second = 0;
  }else{
    printf("%d release %d error.\n", clt, (int)lid);
    ret = lock_protocol::RPCERR;
  }
  return ret;
}
