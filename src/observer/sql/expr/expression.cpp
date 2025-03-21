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
// Created by Wangyunlai on 2022/07/05.
//

#include "sql/expr/expression.h"
#include "sql/expr/tuple.h"
#include <regex>

using namespace std;

RC FieldExpr::get_value(const Tuple &tuple, Value &value) const
{
  return tuple.find_cell(TupleCellSpec(table_name(), field_name()), value);
}

RC ValueExpr::get_value(const Tuple &tuple, Value &value) const
{
  value = value_;
  return RC::SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////////
CastExpr::CastExpr(unique_ptr<Expression> child, AttrType cast_type)
    : child_(std::move(child)), cast_type_(cast_type)
{}

CastExpr::~CastExpr()
{}

RC CastExpr::cast(const Value &value, Value &cast_value) const
{
  RC rc = RC::SUCCESS;
  if (this->value_type() == value.attr_type()) {
    cast_value = value;
    return rc;
  }

  switch (cast_type_) {
    case BOOLEANS: {
      bool val = value.get_boolean();
      cast_value.set_boolean(val);
    } break;
    default: {
      rc = RC::INTERNAL;
      LOG_WARN("unsupported convert from type %d to %d", child_->value_type(), cast_type_);
    }
  }
  return rc;
}

RC CastExpr::get_value(const Tuple &tuple, Value &cell) const
{
  RC rc = child_->get_value(tuple, cell);
  if (rc != RC::SUCCESS) {
    return rc;
  }

  return cast(cell, cell);
}

RC CastExpr::try_get_value(Value &value) const
{
  RC rc = child_->try_get_value(value);
  if (rc != RC::SUCCESS) {
    return rc;
  }

  return cast(value, value);
}

////////////////////////////////////////////////////////////////////////////////

ComparisonExpr::ComparisonExpr(CompOp comp, unique_ptr<Expression> left, unique_ptr<Expression> right)
    : comp_(comp), left_(std::move(left)), right_(std::move(right))
{
    referred_tables_ = left_->referred_tables();
    std::vector<std::string> right_tables = right_->referred_tables();
    referred_tables_.insert(referred_tables_.end(), std::make_move_iterator(right_tables.begin()), 
      std::make_move_iterator(right_tables.end()));
}

ComparisonExpr::~ComparisonExpr()
{}

RC ComparisonExpr::compare_value(const Value &left, const Value &right, bool &result) const
{
  RC rc = RC::SUCCESS;

  if (left.attr_type() == NULL_TYPE || right.attr_type() == NULL_TYPE) {
    result = false;  // NULL无论怎么比较都是false
    return rc;
  }

  int cmp_result = left.compare(right);
  result = false;

  switch (comp_) {
    case EQUAL_TO: {
      result = (0 == cmp_result);
    } break;
    case LESS_EQUAL: {
      result = (cmp_result <= 0);
    } break;
    case NOT_EQUAL: {
      result = (cmp_result != 0);
    } break;
    case LESS_THAN: {
      result = (cmp_result < 0);
    } break;
    case GREAT_EQUAL: {
      result = (cmp_result >= 0);
    } break;
    case GREAT_THAN: {
      result = (cmp_result > 0);
    } break;
    default: {
      LOG_WARN("unsupported comparison. %d", comp_);
      rc = RC::INTERNAL;
    } break;
  }

  return rc;
}

RC ComparisonExpr::try_get_value(Value &cell) const
{
  if (left_->type() == ExprType::VALUE && right_->type() == ExprType::VALUE) {
    ValueExpr *left_value_expr = static_cast<ValueExpr *>(left_.get());
    ValueExpr *right_value_expr = static_cast<ValueExpr *>(right_.get());
    const Value &left_cell = left_value_expr->get_value();
    const Value &right_cell = right_value_expr->get_value();

    bool value = false;
    RC rc = compare_value(left_cell, right_cell, value);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to compare tuple cells. rc=%s", strrc(rc));
    } else {
      cell.set_boolean(value);
    }
    return rc;
  }

  return RC::INVALID_ARGUMENT;
}

RC ComparisonExpr::get_value(const Tuple &tuple, Value &value) const
{
  Value left_value;
  Value right_value;

  RC rc = left_->get_value(tuple, left_value);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of left expression. rc=%s", strrc(rc));
    return rc;
  }
  rc = right_->get_value(tuple, right_value);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of right expression. rc=%s", strrc(rc));
    return rc;
  }

  bool bool_value = false;
  rc = compare_value(left_value, right_value, bool_value);
  if (rc == RC::SUCCESS) {
    value.set_boolean(bool_value);
  }
  return rc;
}

////////////////////////////////////////////////////////////////////////////////
ConjunctionExpr::ConjunctionExpr(Type type, vector<unique_ptr<Expression>> &children)
    : conjunction_type_(type), children_(std::move(children))
{
  std::vector<std::string> tables;
  for (const unique_ptr<Expression> &expr : children_) {
    std::vector<std::string> child_tables = expr->referred_tables();
    tables.insert(tables.end(), std::make_move_iterator(child_tables.begin()), 
      std::make_move_iterator(child_tables.end()));
  }
  referred_tables_ = std::move(tables);
}

RC ConjunctionExpr::get_value(const Tuple &tuple, Value &value) const
{
  RC rc = RC::SUCCESS;
  if (children_.empty()) {
    value.set_boolean(true);
    return rc;
  }

  Value tmp_value;
  for (const unique_ptr<Expression> &expr : children_) {
    rc = expr->get_value(tuple, tmp_value);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to get value by child expression. rc=%s", strrc(rc));
      return rc;
    }
    bool bool_value = tmp_value.get_boolean();
    if ((conjunction_type_ == Type::AND && !bool_value) || (conjunction_type_ == Type::OR && bool_value)) {
      value.set_boolean(bool_value);
      return rc;
    }
  }

  bool default_value = (conjunction_type_ == Type::AND);
  value.set_boolean(default_value);
  return rc;
}

////////////////////////////////////////////////////////////////////////////////

ArithmeticExpr::ArithmeticExpr(ArithmeticExpr::Type type, Expression *left, Expression *right)
    : arithmetic_type_(type), left_(left), right_(right)
{
  std::vector<std::string> tables;
  if (left_ != nullptr) {
  std::vector<std::string> left_tables = left_->referred_tables();
  tables.insert(tables.end(), std::make_move_iterator(left_tables.begin()), 
    std::make_move_iterator(left_tables.end()));
  }
  if (right_ != nullptr) {
    std::vector<std::string> right_tables = right_->referred_tables();
    tables.insert(tables.end(), std::make_move_iterator(right_tables.begin()), 
      std::make_move_iterator(right_tables.end()));
  }
  referred_tables_ = std::move(tables);
}

ArithmeticExpr::ArithmeticExpr(ArithmeticExpr::Type type, unique_ptr<Expression> left, unique_ptr<Expression> right)
    : arithmetic_type_(type), left_(std::move(left)), right_(std::move(right))
{
  std::vector<std::string> tables;
  if (left_ != nullptr) {
  std::vector<std::string> left_tables = left_->referred_tables();
  tables.insert(tables.end(), std::make_move_iterator(left_tables.begin()), 
    std::make_move_iterator(left_tables.end()));
  }
  if (right_ != nullptr) {
    std::vector<std::string> right_tables = right_->referred_tables();
    tables.insert(tables.end(), std::make_move_iterator(right_tables.begin()), 
      std::make_move_iterator(right_tables.end()));
  }
  referred_tables_ = std::move(tables);
}

AttrType ArithmeticExpr::value_type() const
{
  if (!right_) {
    return left_->value_type();
  }

  if (left_->value_type() == AttrType::INTS &&
      right_->value_type() == AttrType::INTS &&
      arithmetic_type_ != Type::DIV) {
    return AttrType::INTS;
  }
  
  return AttrType::FLOATS;
}

ExprValueAttr ArithmeticExpr::value_attr() const 
{
  if (!right_) {
    return left_->value_attr();
  }

  ExprValueAttr left_attr = left_->value_attr();
  ExprValueAttr right_attr = right_->value_attr();

  ExprValueAttr attr;
  attr.nullable = true;
  attr.length = 4;

  if (left_attr.type == AttrType::INTS && right_attr.type == AttrType::INTS && arithmetic_type_ != Type::DIV)
    attr.type = AttrType::INTS;
  else
    attr.type = AttrType::FLOATS;

  return attr;
}

RC ArithmeticExpr::calc_value(const Value &left_value, const Value &right_value, Value &value) const
{
  RC rc = RC::SUCCESS;

  if (left_value.is_null_value() || right_value.is_null_value())
  {
    value.set_null();
    return rc;
  }

  const AttrType target_type = value_type();

  switch (arithmetic_type_) {
    case Type::ADD: {
      if (target_type == AttrType::INTS) {
        value.set_int(left_value.get_int() + right_value.get_int());
      } else {
        value.set_float(left_value.get_float() + right_value.get_float());
      }
    } break;

    case Type::SUB: {
      if (target_type == AttrType::INTS) {
        value.set_int(left_value.get_int() - right_value.get_int());
      } else {
        value.set_float(left_value.get_float() - right_value.get_float());
      }
    } break;

    case Type::MUL: {
      if (target_type == AttrType::INTS) {
        value.set_int(left_value.get_int() * right_value.get_int());
      } else {
        value.set_float(left_value.get_float() * right_value.get_float());
      }
    } break;

    case Type::DIV: {
      if (target_type == AttrType::INTS) {
        if (right_value.get_int() == 0) {
          value.set_null();
        } else {
          value.set_int(left_value.get_int() / right_value.get_int());
        }
      } else {
        if (right_value.get_float() > -EPSILON && right_value.get_float() < EPSILON) {
          value.set_null();
        } else {
          value.set_float(left_value.get_float() / right_value.get_float());
        }
      }
    } break;

    case Type::NEGATIVE: {
      if (target_type == AttrType::INTS) {
        value.set_int(-left_value.get_int());
      } else {
        value.set_float(-left_value.get_float());
      }
    } break;

    default: {
      rc = RC::INTERNAL;
      LOG_WARN("unsupported arithmetic type. %d", arithmetic_type_);
    } break;
  }
  return rc;
}

RC ArithmeticExpr::get_value(const Tuple &tuple, Value &value) const
{
  RC rc = RC::SUCCESS;

  Value left_value;
  Value right_value;

  rc = left_->get_value(tuple, left_value);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of left expression. rc=%s", strrc(rc));
    return rc;
  }

  if (arithmetic_type_ != Type::NEGATIVE) {
    rc = right_->get_value(tuple, right_value);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to get value of right expression. rc=%s", strrc(rc));
      return rc;
    }
  }
  
  return calc_value(left_value, right_value, value);
}

RC ArithmeticExpr::try_get_value(Value &value) const
{
  RC rc = RC::SUCCESS;

  Value left_value;
  Value right_value;

  rc = left_->try_get_value(left_value);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of left expression. rc=%s", strrc(rc));
    return rc;
  }

  if (right_) {
    rc = right_->try_get_value(right_value);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to get value of right expression. rc=%s", strrc(rc));
      return rc;
    }
  }

  return calc_value(left_value, right_value, value);
}

////////////////////////////////////////////////////////////////////////////////

RC CorrelateExpr::get_value(const Tuple &tuple, Value &value) const
{
  value = value_;
  return RC::SUCCESS;
}

RC CorrelateExpr::set_value_from_tuple(const Tuple &tuple)
{
  return tuple.find_cell(correlate_field_.to_tuple_cell_spec(), value_);
}

////////////////////////////////////////////////////////////////////////////////

RC IdentifierExpr::get_value(const Tuple &tuple, Value &value) const
{
  return tuple.find_cell(field_.to_tuple_cell_spec(), value);
}

////////////////////////////////////////////////////////////////////////////////

RC PredNullExpr::get_value(const Tuple &tuple, Value &value) const
{
  Value child_val;
  RC rc = child_->get_value(tuple, child_val);
  if (rc != RC::SUCCESS)
    return rc;
  bool bool_val = op_ == IS_NULL ? child_val.is_null_value() : !child_val.is_null_value();
  value.set_boolean(bool_val);
  return RC::SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////

RC LikeExpr::get_value(const Tuple &tuple, Value &value) const
{
  Value child_val;
 
  RC rc = child_->get_value(tuple, child_val);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of field. rc=%s", strrc(rc));
    return rc;
  }
  bool bool_value ;
  //if(value.type!= CHAR)
  // return re::INVALID_ARGUMENT;
  rc = regex_value(child_val.get_string(), like_pattern_, bool_value);
  bool_value = is_not_like_? !bool_value:bool_value;
  value.set_boolean(bool_value);

  return rc;
}

RC LikeExpr::regex_value(const std::string &input_string, const std::string &like_pattern, bool &value) const
{
    std::string result_pattern;
    std::string temp_pattern;
    size_t startPos = 0;
    size_t found = like_pattern.find('%');

    while (found != std::string::npos) {
        temp_pattern += like_pattern.substr(startPos, found - startPos);
        temp_pattern += ".*";
        startPos = found + 1;
        found = like_pattern.find('%', startPos);
    }
    temp_pattern += like_pattern.substr(startPos);


    startPos = 0;
    found = temp_pattern.find('_');
    while (found != std::string::npos) {
        result_pattern += temp_pattern.substr(startPos, found - startPos);
        result_pattern += ".";
        startPos = found + 1;
        found = temp_pattern.find('_', startPos);
    }

    result_pattern += temp_pattern.substr(startPos);
    // 构建正则表达式
    std::regex like_regex(result_pattern);

    // 执行匹配
    if (std::regex_match(input_string, like_regex)) {
        LOG_INFO("Matched the string: %s", input_string.c_str());
        value = true;
    } else {
        LOG_INFO("Not matched the string: %s", input_string.c_str());
        value = false;
    }

  return RC::SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////

RC QuantifiedCompExprSetExpr::get_value(const Tuple &tuple, Value &value) const 
{
  Value child_value;
  RC rc = child_->get_value(tuple, child_value);
  HANDLE_RC(rc);

  bool is_in = false;
  for (const unique_ptr<Expression> &expr : set_) 
  {
    Value tmp_value;
    rc = expr->get_value(tuple, tmp_value);
    HANDLE_RC(rc);

    int cmp_result = child_value.compare(tmp_value);
    if (cmp_result == 0) 
    {
      is_in = true;
      break;
    }
  }

  value.set_boolean(op_ == QuantifiedComp::IN ? is_in : !is_in);
  return RC::SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////

RC FunctionExpr::get_value(const Tuple &tuple, Value &value) const
{
  RC rc = RC::SUCCESS;
  Value func_arg;
  if (!is_const_) {
    rc = tuple.find_cell(func_arg_.to_tuple_cell_spec(), func_arg);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to get value of function argument. rc=%s", strrc(rc));
      return rc;
    }
  }
  rc = kernel_->do_func(func_arg, value);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to do function. rc=%s", strrc(rc));
    return rc;
  }
  return RC::SUCCESS;
}

ExprValueAttr FunctionExpr::value_attr() const 
{
  return kernel_->get_value_attr();
}