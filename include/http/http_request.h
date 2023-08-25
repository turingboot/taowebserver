#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>

#include "../buffer/buffer.h"

class HttpRequest
{
public:
    enum PARSE_STATE
    {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,
    };

    enum HTTP_CODE
    {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURSE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };

    HttpRequest() { init(); };
    ~HttpRequest() = default;

    void init();
    bool parse(Buffer &buff); // 解析HTTP请求

    // 获取HTTP信息
    std::string path() const;
    std::string &path();
    std::string method() const;
    std::string version() const;
    std::string getPost(const std::string &key) const;
    std::string getPost(const char *key) const;

    bool isKeepAlive() const;

private:
    bool parseRequestLine_(const std::string &line);   // 解析请求行
    void parseRequestHeader_(const std::string &line); // 解析请求头
    void parseDataBody_(const std::string &line);      // 解析数据体

    void parsePath_();
    void parsePost_();

    static int convertHex(char ch);

    PARSE_STATE state_;
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> post_;
    
    //处理逻辑HTML的逻辑跳转
    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
};


const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML{
            "/login", "/index"};

const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG {
        {"/register.html", 0}, {"/doLogin", 1},  };

void HttpRequest::init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::isKeepAlive() const {
    if(header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n";
    if(buff.readableBytes() <= 0) {
        return false;
    }
    //std::cout<<"parse buff start:"<<std::endl;
    //buff.printContent();
    //std::cout<<"parse buff finish:"<<std::endl;
    while(buff.readableBytes() && state_ != FINISH) {
        const char* lineEnd = std::search(buff.curReadPtr(), buff.curWritePtrConst(), CRLF, CRLF + 2);
        std::string line(buff.curReadPtr(), lineEnd);
        switch(state_)
        {
        case REQUEST_LINE:
            //std::cout<<"REQUEST: "<<line<<std::endl;
            if(!parseRequestLine_(line)) {
                return false;
            }
            parsePath_();
            break;    
        case HEADERS:
            parseRequestHeader_(line);
            if(buff.readableBytes() <= 2) {
                state_ = FINISH;
            }
            break;
        case BODY:
            parseDataBody_(line);
            break;
        default:
            break;
        }
        if(lineEnd == buff.curWritePtr()) { break; }
        buff.updateReadPtrUntilEnd(lineEnd + 2);
    }
    return true;
}

void HttpRequest::parsePath_() {
    if(path_ == "/") {
        path_ = "/login.html"; 
    } else if(path_ == "/doLogin") {
         path_ = path_; 
    }
    else {
        for(auto &item: DEFAULT_HTML) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}

bool HttpRequest::parseRequestLine_(const std::string& line) {
    std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch subMatch;
    if(regex_match(line, subMatch, patten)) {   
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS;
        return true;
    }
    return false;
}

void HttpRequest::parseRequestHeader_(const std::string& line) {
    std::regex patten("^([^:]*): ?(.*)$");
    std::smatch subMatch;
    if(regex_match(line, subMatch, patten)) {
        header_[subMatch[1]] = subMatch[2];
    }
    else {
        state_ = BODY;
    }
}

void HttpRequest::parseDataBody_(const std::string& line) {
    body_ = line;
    parsePost_();
    state_ = FINISH;
}

int HttpRequest::convertHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch;
}

void HttpRequest::parsePost_() {
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        if(body_.size() == 0) { return; }
        

        //解析body 获取到body里面的值
        std::string key, value;
        int num = 0;
        int n = body_.size();
        int i = 0, j = 0;

        for(; i < n; i++) {
            char ch = body_[i];
            switch (ch) {
            case '=':
                key = body_.substr(j, i - j);
                j = i + 1;
                break;
            case '+':
                body_[i] = ' ';
                break;
            case '%':
                num = convertHex(body_[i + 1]) * 16 + convertHex(body_[i + 2]);
                body_[i + 2] = num % 10 + '0';
                body_[i + 1] = num / 10 + '0';
                i += 2;
                break;
            case '&':
                value = body_.substr(j, i - j);
                j = i + 1;
                post_[key] = value;
                break;
            default:
                break;
            }
        }

        assert(j <= i);
        if(post_.count(key) == 0 && j < i) {
            value = body_.substr(j, i - j);
            post_[key] = value;
        }

        //验证用户登录逻辑
        if(DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            spdlog::info("HTML_TAG===>{}",tag);
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);
                if(isLogin && post_["username"]=="admin" && post_["password"]=="123456") {
                    path_ = "/index.html";
                } 
                // else {
                //     // path_ = "/error.html";
                // }
        }
    }

    }   
}

std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::getPost(const std::string& key) const {
    
    if(key == "") return "";

    //解析出来Post数据,如果查到到则就返回,没有返回空值
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::getPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

#endif