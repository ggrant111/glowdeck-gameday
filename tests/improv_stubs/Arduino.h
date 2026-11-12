#pragma once
#include <cstdint>
#include <string>
#include <utility>
class String {
 public:
  String()=default;
  String(const char *s):value_(s?s:""){}
  String(const std::string &s):value_(s){}
  explicit String(int n):value_(std::to_string(n)){}
  size_t length() const {return value_.size();}
  const char *c_str() const {return value_.c_str();}
  void replace(const char *a,const String &b) {
    std::string old(a);
    size_t pos=0;
    while((pos=value_.find(old,pos))!=std::string::npos){
      value_.replace(pos,old.size(),b.value_);
      pos+=b.value_.size();
    }
  }
  std::string str() const {return value_;}
 private:
  std::string value_;
};
