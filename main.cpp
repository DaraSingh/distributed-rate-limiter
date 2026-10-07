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
        // 1) LOAD the script into Redis once (returns its SHA)
        // string sha = redis.script_load(load_file("script.lua"));
        string sha = redis.script_load(load_file("token_bucket.lua"));
        long long now_s = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
        long long start_s = (now_s / 10 + 1) * 10;      // next 10-second boundary
        if (start_s - now_s < 3) start_s += 10;          // make sure there's time for every container to reach this point
        this_thread::sleep_until(system_clock::time_point(seconds(start_s)));
        Redis redis("tcp://" + host + ":6379");

        int allowed=0;
        int rejected=0;
        for (int i = 1; i <= 20000; ++i) {
            // 2) CALL the script for each request
            // long long r = redis.evalsha<long long>(sha, {"user:1"}, {"100", "60"});
            long long r = redis.evalsha<long long>(sha, {"bucket:1"}, {"100", "0.1","1"});
            if(r==1) allowed++;
            else rejected++;
            // cout << "request " << i << ": " << (r == 1 ? "allowed" : "rejected") << endl;
        }
        cout<<"Allowed : "<<allowed<<endl;
        cout<<"Rejected : "<<rejected<<endl;
        redis.set("Hello","World 2000");
        auto val=redis.get("Hello");
        if(val) cout<<*val<<endl;
    }
    catch (const Error& e)
    {
        cout<<"Redis Error => "<<e.what()<<endl;
    }
}