#include "../Native/ClientLaunchIdentity.h"
#include <cassert>
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>
int main(){
 const std::wstring code(43,L'a');std::wstring output;Hollowwake::ClientLaunchIdentity state;
 assert(!state.Consume(0,output));assert(!state.Configure(code+L"\" -other",30,100));assert(!state.Configure(code,301,100));
 assert(state.Configure(code,2,100));assert(!state.Consume(2100,output));assert(output.empty());
 assert(state.Configure(code,300,5000));std::atomic<int> winners{0};std::vector<std::thread> threads;
 for(int i=0;i<16;i++)threads.emplace_back([&](){std::wstring command;if(state.Consume(6000,command)){assert(command==L" -AUTH_LOGIN=unused -AUTH_TYPE=exchangecode -AUTH_PASSWORD="+code);++winners;}});
 for(auto& t:threads)t.join();assert(winners==1);assert(!state.Configure(code,300,6001));assert(!state.Consume(6001,output));
 std::cout<<"PASS production game-ticket syntax, expiry, single-consumer concurrency and exact exchange-code command construction; no credentials printed, no game execution\n";
}
