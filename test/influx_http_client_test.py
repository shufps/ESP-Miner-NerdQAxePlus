#!/usr/bin/env python3
"""Compile and execute all of production influx.cpp with SDK transport seams.

No network/hardware I/O. Real ArduinoJson, CSV parsing, and line serialization.
Usage: python3 test/influx_http_client_test.py [source-root]
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
HEADERS = {
    'esp_log.h': '#define ESP_LOGI(...)\n#define ESP_LOGE(...)\n#define ESP_LOGD(...)\n#define ESP_LOGW(...)\n',
    'macros.h': '#include <cstdlib>\n#define MALLOC malloc\n',
    'freertos/FreeRTOS.h': '', 'freertos/semphr.h': '', 'freertos/task.h': '',
    'psram_allocator.h': '''#include "ArduinoJson.h"
#include <cstdlib>
struct PSRAMAllocator : ArduinoJson::Allocator {
 void* allocate(size_t n) override {return malloc(n);}
 void deallocate(void* p) override {free(p);}
 void* reallocate(void* p, size_t n) override {return realloc(p,n);}
};''',
    'esp_crt_bundle.h': '#pragma once\ninline int esp_crt_bundle_attach(void*) {return 0;}\n',
    'esp_http_client.h': r'''#pragma once
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include "esp_crt_bundle.h"
#define CHECK(x) do {if (!(x)) {fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}} while(0)
using esp_err_t = int;
constexpr int ESP_OK = 0;
enum esp_http_client_method_t { HTTP_METHOD_GET, HTTP_METHOD_POST };
struct esp_http_client_config_t {
 const char *url = nullptr;
 esp_http_client_method_t method = HTTP_METHOD_GET;
 int (*crt_bundle_attach)(void*) = nullptr;
 bool skip_cert_common_name_check = false;
 const char *common_name = nullptr;
};
struct Client { std::string url, response; int status = 200; };
using esp_http_client_handle_t = Client*;
inline bool fail_init = false;
inline std::vector<std::string> urls;
inline std::string last_post;
inline const char* esp_err_to_name(int) {return "test";}
inline Client* esp_http_client_init(const esp_http_client_config_t* cfg) {
 // Applied to EVERY call, for HTTP as well as HTTPS. SDK ignores bundle on HTTP.
 CHECK(cfg->crt_bundle_attach == esp_crt_bundle_attach);
 CHECK(!cfg->skip_cert_common_name_check);
 CHECK(cfg->common_name == nullptr);
 urls.emplace_back(cfg->url);
 if(fail_init) return nullptr;
 auto c = new Client{cfg->url, ""};
 if(c->url.find("/orgs?") != std::string::npos) {c->response=R"({"orgs":[{"id":"org-id"}]})"; CHECK(cfg->method==HTTP_METHOD_GET);}
 else if(c->url.find("/buckets?") != std::string::npos) {c->response=R"({"buckets":[{"id":"bucket-id"}]})"; CHECK(cfg->method==HTTP_METHOD_GET);}
 else if(c->url.find("/buckets") != std::string::npos) {c->status=201; CHECK(cfg->method==HTTP_METHOD_POST);}
 else if(c->url.find("/query?") != std::string::npos) {
   CHECK(cfg->method==HTTP_METHOD_POST);
   c->response=",result,table,_start,_stop,_time,_value,_field,_measurement\r\n"
    ",_result,0,start,stop,time,3257420,total_best_difficulty,miner\r\n"
    ",_result,1,start,stop,time,123,total_uptime,miner\r\n"
    ",_result,2,start,stop,time,7,total_blocks_found,miner\r\n";
 } else if(c->url.find("/write?") != std::string::npos) {c->status=204; CHECK(cfg->method==HTTP_METHOD_POST);}
 else {CHECK(c->url.find("/ping") != std::string::npos); CHECK(cfg->method==HTTP_METHOD_GET); c->status=204;}
 return c;
}
inline int esp_http_client_set_header(Client*,const char*,const char*) {return 0;}
inline int esp_http_client_set_post_field(Client*,const char* s,int n) {last_post.assign(s,n);return 0;}
inline int esp_http_client_perform(Client*) {return 0;}
inline int esp_http_client_get_status_code(Client* c) {return c->status;}
inline int esp_http_client_get_content_length(Client* c) {return c->response.size();}
inline int esp_http_client_open(Client*,int) {return 0;}
inline int esp_http_client_fetch_headers(Client* c) {return c->response.size();}
inline int esp_http_client_write(Client*,const char*,int n) {return n;}
inline int esp_http_client_read_response(Client* c,char* b,int n) {int len=c->response.size();CHECK(len<=n);memcpy(b,c->response.data(),len);return len;}
inline int esp_http_client_read(Client* c,char* b,int n) {return esp_http_client_read_response(c,b,n);}
inline int esp_http_client_close(Client*) {return 0;}
inline int esp_http_client_cleanup(Client* c) {delete c;return 0;}
''',
}
MAIN = r'''
#include "influx.h"
#include "esp_http_client.h"
int main() {
 for(const char* scheme : {"http://example.com", "https://example.com"}) {
    Influx db;
    CHECK(db.init(scheme,8086,"fixture-token","telemetry","org","miner"));
    CHECK(db.m_stats.total_best_difficulty==0);
    CHECK(db.ping());
    CHECK(db.bucket_exists());
    CHECK(db.create_bucket()); // exercises organization lookup too
    CHECK(db.load_last_values());
    CHECK(db.m_stats.total_best_difficulty==3257420);
    CHECK(db.m_stats.total_uptime==123);
    CHECK(db.m_stats.total_blocks_found==7);
    db.m_stats.best_difficulty=170794;
    db.m_stats.total_best_difficulty=9158823;
    db.write(db.m_stats);
    CHECK(last_post.find("best_difficulty=170794.000000,total_best_difficulty=9158823.000000")!=std::string::npos);
    CHECK(last_post.find("total_uptime=123")!=std::string::npos);
    CHECK(last_post.find("total_blocks_found=7")!=std::string::npos);
    CHECK(urls.size()==6);
    urls.clear();
    fail_init=true;
    CHECK(!db.ping()); CHECK(!db.bucket_exists()); CHECK(!db.create_bucket()); CHECK(!db.load_last_values()); db.write(db.m_stats);
    fail_init=false;
    CHECK(urls.size()==5); // create returns early on failed org lookup
    urls.clear();
 }
 puts("PASS: complete production influx.cpp, all six HTTP/HTTPS configs verified, init-failure exits, CSV load and wire serialization");
}
'''
with tempfile.TemporaryDirectory(prefix='influx-http-', dir=os.environ.get('TMPDIR')) as tmp:
    tmp = Path(tmp)
    for name, content in HEADERS.items():
        file = tmp / name
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(content)
    (tmp / 'main.cpp').write_text(MAIN)
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Wno-unused-variable',
                    '-I', str(tmp), '-I', str(ROOT / 'components/influx/include'),
                    '-I', str(ROOT / 'components/arduinojson'),
                    str(ROOT / 'components/influx/influx.cpp'), str(tmp / 'main.cpp'),
                    '-o', str(tmp / 'test')], check=True)
    subprocess.run([str(tmp / 'test')], check=True)
