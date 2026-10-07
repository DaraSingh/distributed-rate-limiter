#include <sw/redis++/redis++.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <thread>
#include<chrono>

using namespace sw::redis;
using namespace std;
using namespace::chrono;

string load_file(const string& path) {
    ifstream f(path);                  // open the file
    stringstream ss;
    ss << f.rdbuf();                   // pour the file's content into ss
    return ss.str();                   // give back all the text
}

int main(){
    const char* h = getenv("REDIS_HOST");
    string host = h ? h : "127.0.0.1";
    try{
        Redis redis("tcp://" + host + ":6379");
        string sha = redis.script_load(load_file("token_bucket.lua"));
        long long now_s = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
        long long start_s = (now_s / 10 + 1) * 10;      // next 10-second boundary
        if (start_s - now_s < 3) start_s += 10;          // make sure there's time for every container to reach this point
        this_thread::sleep_until(system_clock::time_point(seconds(start_s)));

        int allowed=0;
        int rejected=0;
        for (int i = 1; i <= 20000; ++i) {
            // KEYS: bucket:1 | ARGV: capacity=100, refill=0.1 tokens/sec, requested=1 token
            long long r = redis.evalsha<long long>(sha, {"bucket:1"}, {"100", "0.1","1"});
            if(r==1) allowed++;
            else rejected++;
        }
        cout<<"Allowed : "<<allowed<<endl;
        cout<<"Rejected : "<<rejected<<endl;
    }
    catch (const Error& e)
    {
        cout<<"Redis Error => "<<e.what()<<endl;
    }
}