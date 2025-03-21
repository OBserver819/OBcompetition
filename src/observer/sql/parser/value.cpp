/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by WangYunlai on 2023/06/28.
//

#include <sstream>
#include "sql/parser/value.h"
#include "storage/field/field.h"
#include "common/log/log.h"
#include "common/lang/comparator.h"
#include "common/lang/string.h"

const char *ATTR_TYPE_NAME[] = {"undefined", "chars", "ints", "floats", "booleans", "null_type", "dates", "texts"};

const char *attr_type_to_string(AttrType type)
{
  if (type >= UNDEFINED && type <= TEXTS) {
    return ATTR_TYPE_NAME[type];
  }
  return "unknown";
}
AttrType attr_type_from_string(const char *s)
{
  for (unsigned int i = 0; i < sizeof(ATTR_TYPE_NAME) / sizeof(ATTR_TYPE_NAME[0]); i++) {
    if (0 == strcmp(ATTR_TYPE_NAME[i], s)) {
      return (AttrType)i;
    }
  }
  return UNDEFINED;
}

Value::Value(int val, AttrType type){
  if(type == AttrType::DATES){
    set_date(val);
  }
}

Value::Value(int val)
{
  set_int(val);
}

Value::Value(float val)
{
  set_float(val);
}

Value::Value(bool val)
{
  set_boolean(val);
}

Value::Value(const char *s, int len /*= 0*/)
{
  set_string(s, len);
}

void Value::set_data(char *data, int length)
{
  switch (attr_type_) {
    case CHARS: {
      set_string(data, length);
    } break;
    case TEXTS: {
      set_text_string(data, length);
    } break;
    case DATES:
    case INTS: {
      num_value_.int_value_ = *(int *)data;
      length_ = length;
    } break;
    case FLOATS: {
      num_value_.float_value_ = *(float *)data;
      length_ = length;
    } break;
    case BOOLEANS: {
      num_value_.bool_value_ = *(int *)data != 0;
      length_ = length;
    } break;
    case NULL_TYPE:
      break;
    default: {
      LOG_WARN("unknown data type: %d", attr_type_);
    } break;
  }
}
void Value::set_int(int val)
{
  attr_type_ = INTS;
  num_value_.int_value_ = val;
  length_ = sizeof(val);
}

void Value::set_date(int val){
  attr_type_ = DATES;
  num_value_.int_value_ = val;
  length_ = sizeof(val);
}

void Value::set_float(float val)
{
  attr_type_ = FLOATS;
  num_value_.float_value_ = val;
  length_ = sizeof(val);
}
void Value::set_boolean(bool val)
{
  attr_type_ = BOOLEANS;
  num_value_.bool_value_ = val;
  length_ = sizeof(val);
}
void Value::set_string(const char *s, int len /*= 0*/)
{
  attr_type_ = CHARS;
  if (len > 0) {
    len = strnlen(s, len);
    str_value_.assign(s, len);
  } else {
    str_value_.assign(s);
  }
  length_ = str_value_.length();
}
void Value::set_text_string(const char *s, int len /*= 0*/)
{
  attr_type_ = TEXTS;
  if (len > 0) {
    len = strnlen(s, len);
    str_value_.assign(s, len);
  } else {
    str_value_.assign(s);
  }
  length_ = str_value_.length();
}

void Value::set_value(const Value &value)
{
  switch (value.attr_type_) {
    case INTS: {
      set_int(value.get_int());
    } break;
    case FLOATS: {
      set_float(value.get_float());
    } break;
    case CHARS: {
      set_string(value.get_string().c_str());
    } break;
    case BOOLEANS: {
      set_boolean(value.get_boolean());
    } break;
    case UNDEFINED: {
      ASSERT(false, "got an invalid value type");
    } break;
    case DATES: {
      set_date(value.get_int());
    } break;
    case TEXTS: {
      set_text_string(value.get_text_string().c_str());
    } break;
    case NULL_TYPE:{
      attr_type_ = NULL_TYPE;
    } break;
  }
}

void Value::cast_to(AttrType attrtype)
{
  switch (attrtype) {
    case INTS: {
      set_int(get_int());
    } break;
    case FLOATS: {
      set_float(get_float());
    } break;
    case CHARS: {
      set_string(get_string().c_str());
    } break;
    case BOOLEANS: {
      set_boolean(get_boolean());
    } break;
    case UNDEFINED: {
      ASSERT(false, "got an invalid value type");
    } break;
    case DATES: {
      set_date(get_int());
    } break;
    case TEXTS: {
      set_text_string(get_string().c_str());
    } break;
    case NULL_TYPE:{
      attr_type_ = NULL_TYPE;
    } break;
  }
}

const char *Value::data() const
{
  switch (attr_type_) {
    case CHARS: {
      return str_value_.c_str();
    } break;
    default: {
      return (const char *)&num_value_;
    } break;
  }
}

// 什么时候使用这个to_string
// A: communicator展示数据的时候
std::string Value::to_string() const
{
  std::stringstream os;
  switch (attr_type_) {
    case DATES:{
      int year = num_value_.int_value_ / 10000;
      int month = (num_value_.int_value_ % 10000) / 100;
      int day = num_value_.int_value_ % 100;

      std::string month_string = std::to_string(month);
      if(month < 10){
        month_string.insert(0, 1, '0');
      }

      std::string day_string = std::to_string(day);
      if( day < 10){
        day_string.insert(0, 1, '0');
      }
      
      os << year <<"-"<<month_string<<"-"<<day_string;
    }break;
    case INTS: {
      os << num_value_.int_value_;
    } break;
    case FLOATS: {
      os << common::double_to_str(num_value_.float_value_);
    } break;
    case BOOLEANS: {
      os << num_value_.bool_value_;
    } break;
    case CHARS: {
      os << str_value_;
    } break;
    case NULL_TYPE: {
      os << "NULL";
    } break;
    case TEXTS: {
      os << str_value_;
    } break;
    default: {
      LOG_WARN("unsupported attr type: %d", attr_type_);
    } break;
  }
  return os.str();
}

std::string Value::get_text_md5() const{
    unsigned char digest[16];
    common::MD5String(const_cast<char *>(str_value_.c_str()), digest);
    std::string result;
    for (int i = 0; i < 16; i++) {
        char buf[3];
        sprintf(buf, "%02X", digest[i]);
        result += buf;
    }
    return result;
}

int Value::compare(const Value &other) const
{
  if (this->attr_type_ == other.attr_type_) {
    switch (this->attr_type_) {
      // 本质上date也是int，跟int的比较一样
      case DATES: 
      case INTS: {
        return common::compare_int((void *)&this->num_value_.int_value_, (void *)&other.num_value_.int_value_);
      } break;
      case FLOATS: {
        return common::compare_float((void *)&this->num_value_.float_value_, (void *)&other.num_value_.float_value_);
      } break;
      case CHARS: {
        return common::compare_string((void *)this->str_value_.c_str(),
            this->str_value_.length(),
            (void *)other.str_value_.c_str(),
            other.str_value_.length());
      } break;
      case TEXTS: {
        return common::compare_string((void *)this->str_value_.c_str(),
            this->str_value_.length(),
            (void *)other.str_value_.c_str(),
            other.str_value_.length());
      } break;
      case BOOLEANS: {
        return this->num_value_.bool_value_ == other.num_value_.bool_value_ ? 0 : -1;
      }
      case NULL_TYPE:
        return 0;  // 这里是三路比较，但NULL值的比较不满足三路比较（无论如何都是false），因此只能在compare的调用者处再做特判了，这里随便返回。
      default: {
        LOG_WARN("unsupported type: %d", this->attr_type_);
      }
    }
  // else if (this->attr_type_ == INTS && other.attr_type_ == FLOATS) {
  //   float this_data = this->num_value_.int_value_;
  //   return common::compare_float((void *)&this_data, (void *)&other.num_value_.float_value_);
  // } else if (this->attr_type_ == FLOATS && other.attr_type_ == INTS) {
  //   float other_data = other.num_value_.int_value_;
  //   return common::compare_float((void *)&this->num_value_.float_value_, (void *)&other_data);
  // } 

  } else if (this->attr_type_ == NULL_TYPE) {
    return -1;
  } else if (other.attr_type_ == NULL_TYPE) {
    return 1;
  }
  else if(this->attr_type_ == FLOATS || other.attr_type_ == FLOATS){
    float this_data = this->get_float();
    float other_data = other.get_float();
    return common::compare_float((void *)&this_data, (void *)&other_data);
  }
  else if(this->attr_type_ == CHARS && other.attr_type_ == INTS){
    int this_data = this->get_int();
    return common::compare_int((void *)&this_data, (void *)&other.num_value_.int_value_);
  }
  else if(this->attr_type_ == INTS && other.attr_type_ == CHARS){
    int other_data = other.get_int();
    return common::compare_int((void *)&this->num_value_.int_value_, (void *)&other_data);
  }

  LOG_WARN("not supported");
  return -1;  // TODO return rc?
}

int Value::get_int() const
{
  switch (attr_type_) {
    case CHARS: {
      try {
        return (int)(std::stof(str_value_) + 0.5);
      } catch (std::exception const &ex) {
        LOG_TRACE("failed to convert string to number. s=%s, ex=%s", str_value_.c_str(), ex.what());
        return 0;
      }
    }
    case DATES:
    case INTS: {
      return num_value_.int_value_;
    }
    case FLOATS: {
      // 此处需要对num_value_.float_value_四舍五入
      return (int)(num_value_.float_value_ + 0.5);
    }
    case BOOLEANS: {
      return (int)(num_value_.bool_value_);
    }
    default: {
      LOG_WARN("unknown data type. type=%d", attr_type_);
      return 0;
    }
  }
  return 0;
}

float Value::get_float() const
{
  switch (attr_type_) {
    case CHARS: {
      try {
        return std::stof(str_value_);
      } catch (std::exception const &ex) {
        LOG_TRACE("failed to convert string to float. s=%s, ex=%s", str_value_.c_str(), ex.what());
        return 0.0;
      }
    } break;
    case INTS: {
      return float(num_value_.int_value_);
    } break;
    case FLOATS: {
      return num_value_.float_value_;
    } break;
    case BOOLEANS: {
      return float(num_value_.bool_value_);
    } break;
    default: {
      LOG_WARN("unknown data type. type=%d", attr_type_);
      return 0;
    }
  }
  return 0;
}

std::string Value::get_string() const
{
  return this->to_string();
}
std::string Value::get_text_string() const
{
  std::stringstream os;
  std::string file_path = "miniob/db/sys/"+ str_value_;
  std::ifstream fs(file_path);  // 打开文件 example.txt
  if (!fs.is_open()) {
    LOG_ERROR("Failed to open file for read. file name=%s, errmsg=%s", file_path.c_str(), strerror(errno));
    return os.str();
  }
  os << fs.rdbuf();  // 将文件内容读取到字符串流中

  fs.close();  // 关闭文件
  
  return os.str();
}

bool Value::get_boolean() const
{
  switch (attr_type_) {
    case CHARS: {
      try {
        float val = std::stof(str_value_);
        if (val >= EPSILON || val <= -EPSILON) {
          return true;
        }

        int int_val = std::stol(str_value_);
        if (int_val != 0) {
          return true;
        }

        return !str_value_.empty();
      } catch (std::exception const &ex) {
        LOG_TRACE("failed to convert string to float or integer. s=%s, ex=%s", str_value_.c_str(), ex.what());
        return !str_value_.empty();
      }
    } break;
    case INTS: {
      return num_value_.int_value_ != 0;
    } break;
    case FLOATS: {
      float val = num_value_.float_value_;
      return val >= EPSILON || val <= -EPSILON;
    } break;
    case BOOLEANS: {
      return num_value_.bool_value_;
    } break;
    default: {
      LOG_WARN("unknown data type. type=%d", attr_type_);
      return false;
    }
  }
  return false;
}
