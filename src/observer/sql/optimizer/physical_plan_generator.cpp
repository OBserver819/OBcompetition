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
// Created by Wangyunlai on 2022/12/14.
//

#include <utility>

#include "sql/operator/aggregate_logical_operator.h"
#include "sql/operator/aggregate_physical_operator.h"
#include "sql/operator/deduplicate_logical_operator.h"
#include "sql/operator/deduplicate_physical_operator.h"
#include "sql/operator/logical_operator.h"
#include "sql/optimizer/physical_plan_generator.h"
#include "sql/operator/table_get_logical_operator.h"
#include "sql/operator/table_scan_physical_operator.h"
#include "sql/operator/index_scan_physical_operator.h"
#include "sql/operator/predicate_logical_operator.h"
#include "sql/operator/predicate_physical_operator.h"
#include "sql/operator/project_logical_operator.h"
#include "sql/operator/project_physical_operator.h"
#include "sql/operator/insert_logical_operator.h"
#include "sql/operator/insert_physical_operator.h"
#include "sql/operator/delete_logical_operator.h"
#include "sql/operator/delete_physical_operator.h"
#include "sql/operator/update_logical_operator.h"
#include "sql/operator/update_physical_operator.h"
#include "sql/operator/explain_logical_operator.h"
#include "sql/operator/explain_physical_operator.h"
#include "sql/operator/join_logical_operator.h"
#include "sql/operator/join_physical_operator.h"
#include "sql/operator/calc_logical_operator.h"
#include "sql/operator/calc_physical_operator.h"
#include "sql/operator/sort_logical_operator.h"
#include "sql/operator/sort_physical_operator.h"
#include "sql/operator/apply_logical_operator.h"
#include "sql/operator/apply_physical_operator.h"
#include "sql/operator/function_physical_operator.h"
#include "sql/operator/function_logical_operator.h"
#include "sql/operator/field_cul_physical_operator.h"
#include "sql/operator/field_cul_logical_operator.h"
#include "sql/operator/insert_multi_physical_operator.h"
#include "sql/operator/insert_multi_logical_operator.h"
#include "sql/expr/expression.h"
#include "common/log/log.h"

using namespace std;

RC PhysicalPlanGenerator::create(LogicalOperator &logical_operator, unique_ptr<PhysicalOperator> &oper)
{
  RC rc = RC::SUCCESS;

  switch (logical_operator.type()) {
    case LogicalOperatorType::CALC: {
      return create_plan(static_cast<CalcLogicalOperator &>(logical_operator), oper);
    } break;

    case LogicalOperatorType::TABLE_GET: {
      return create_plan(static_cast<TableGetLogicalOperator &>(logical_operator), oper);
    } break;

    case LogicalOperatorType::PREDICATE: {
      return create_plan(static_cast<PredicateLogicalOperator &>(logical_operator), oper);
    } break;

    case LogicalOperatorType::PROJECTION: {
      return create_plan(static_cast<ProjectLogicalOperator &>(logical_operator), oper);
    } break;

    case LogicalOperatorType::INSERT: {
      return create_plan(static_cast<InsertLogicalOperator &>(logical_operator), oper);
    } break;

    case LogicalOperatorType::DELETE: {
      return create_plan(static_cast<DeleteLogicalOperator &>(logical_operator), oper);
    } break;
    case LogicalOperatorType::UPDATE: {
      return create_plan(static_cast<UpdateLogicalOperator &>(logical_operator), oper);
    } break;
    case LogicalOperatorType::EXPLAIN: {
      return create_plan(static_cast<ExplainLogicalOperator &>(logical_operator), oper);
    } break;

    case LogicalOperatorType::JOIN: {
      return create_plan(static_cast<JoinLogicalOperator &>(logical_operator), oper);
    } break;

    case LogicalOperatorType::SORT:{
      return create_plan(static_cast<SortLogicalOperator &>(logical_operator), oper);
    }break;

    case LogicalOperatorType::DEDUPLICATE:{
      return create_plan(static_cast<DeduplicateLogicalOperator &>(logical_operator), oper);
    }break;

    case LogicalOperatorType::AGGREGATE:{
      return create_plan(static_cast<AggregateLogicalOperator &>(logical_operator), oper);
    }break;
    
    case LogicalOperatorType::SCALAR_SUBQUERY: {
      return create_plan(static_cast<ScalarSubqueryLogicalOperator &>(logical_operator), oper);
    } break;

    case LogicalOperatorType::EXISTENTIAL_SUBQUERY: {
      return create_plan(static_cast<ExistentialSubqueryLogicalOperator &>(logical_operator), oper);
    } break;

    case LogicalOperatorType::QUANTIFIEDCOMP_SUBQUERY: {
      return create_plan(static_cast<QuantifiedCompSubqueryLogicalOperator &>(logical_operator), oper);
    } break;

    // 必须先把头文件都引入了，否则会报错Non-const lvalue reference to type 'FunctionLogicalOperator' cannot bind to a value of unrelated type 
    case LogicalOperatorType::FUNCTION: {
      return create_plan(static_cast<FunctionLogicalOperator&>(logical_operator), oper);
    }break;

    case LogicalOperatorType::FIELD_CUL: {
      return create_plan(static_cast<FieldCulLogicalOperator&>(logical_operator), oper);
    }break;

    case LogicalOperatorType::INSERT_MULTI: {
      return create_plan(static_cast<InsertMultiLogicalOperator&>(logical_operator), oper);
    }break;

    default: {
      return RC::INVALID_ARGUMENT;
    }
  }
  return rc;
}

RC PhysicalPlanGenerator::create_plan(SortLogicalOperator &sort_logical_oper, std::unique_ptr<PhysicalOperator> &oper){
  LogicalOperator* child_logical_oper = sort_logical_oper.children().front().get();
  unique_ptr<PhysicalOperator> child_phy_oper;
  RC rc = create(*child_logical_oper, child_phy_oper);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to create child operator of sort operator. rc=%s", strrc(rc));
    return rc;
  }
  oper = make_unique<SortPhysicalOperator>(sort_logical_oper.get_order_fields());
  oper->add_child(std::move(child_phy_oper));
  return rc;
}

RC PhysicalPlanGenerator::create_plan(DeduplicateLogicalOperator& deduplicate_oper, std::unique_ptr<PhysicalOperator> &oper){
  vector<unique_ptr<LogicalOperator>> &child_opers = deduplicate_oper.children();
  unique_ptr<PhysicalOperator> child_phy_oper;

  RC rc = RC::SUCCESS;
  if (!child_opers.empty()) {
    LogicalOperator *child_oper = child_opers.front().get();
    rc = create(*child_oper, child_phy_oper);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to create project logical operator's child physical operator. rc=%s", strrc(rc));
      return rc;
    }
  }

  oper = make_unique<DeduplicateAggPhysicalOperator>(deduplicate_oper.get_deduplicate_fields(), deduplicate_oper.get_only_put_one());
  oper->add_child(std::move(child_phy_oper));
  return rc;
}

RC PhysicalPlanGenerator::create_plan(AggregateLogicalOperator& aggregate_oper, std::unique_ptr<PhysicalOperator> &oper){
  vector<unique_ptr<LogicalOperator>> &child_opers = aggregate_oper.children();
  unique_ptr<PhysicalOperator> child_phy_oper;

  RC rc = RC::SUCCESS;
  if (!child_opers.empty()) {
    LogicalOperator *child_oper = child_opers.front().get();
    rc = create(*child_oper, child_phy_oper);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to create project logical operator's child physical operator. rc=%s", strrc(rc));
      return rc;
    }
  }
  
  oper = make_unique<AggregatePhysicalOperator>(aggregate_oper.fid_, aggregate_oper.virtual_name_, aggregate_oper.group_fids_, aggregate_oper.op_);
  oper->add_child(std::move(child_phy_oper));
  return rc;
}

RC PhysicalPlanGenerator::create_plan(TableGetLogicalOperator &table_get_oper, unique_ptr<PhysicalOperator> &oper)
{
  vector<unique_ptr<Expression>> &predicates = table_get_oper.predicates();
  // 看看是否有可以用于索引查找的表达式
  Table *table = table_get_oper.table();

  Index *index = nullptr;
  ValueExpr *value_expr = nullptr;
  for (auto &expr : predicates) {
    if (expr->type() == ExprType::COMPARISON) {
      auto comparison_expr = static_cast<ComparisonExpr *>(expr.get());
      // 简单处理，就找等值查询
      if (comparison_expr->comp() != EQUAL_TO) {
        continue;
      }

      unique_ptr<Expression> &left_expr = comparison_expr->left();
      unique_ptr<Expression> &right_expr = comparison_expr->right();
      // 左右比较的一边最少是一个值
      if (left_expr->type() != ExprType::VALUE && right_expr->type() != ExprType::VALUE) {
        continue;
      }

      FieldExpr *field_expr = nullptr;
      if (left_expr->type() == ExprType::FIELD) {
        ASSERT(right_expr->type() == ExprType::VALUE, "right expr should be a value expr while left is field expr");
        field_expr = static_cast<FieldExpr *>(left_expr.get());
        value_expr = static_cast<ValueExpr *>(right_expr.get());
      } else if (right_expr->type() == ExprType::FIELD) {
        ASSERT(left_expr->type() == ExprType::VALUE, "left expr should be a value expr while right is a field expr");
        field_expr = static_cast<FieldExpr *>(right_expr.get());
        value_expr = static_cast<ValueExpr *>(left_expr.get());
      }

      if (field_expr == nullptr) {
        continue;
      }

      const Field &field = field_expr->field();
      index = table->find_index_by_field(field.field_name());
      if (nullptr != index) {
        break;
      }
    }
  }

  // 我们根据谓词来查找，看哪个字段有index，就创建一个index
  if (index != nullptr) {
    ASSERT(value_expr != nullptr, "got an index but value expr is null ?");

    const Value &value = value_expr->get_value();
    IndexScanPhysicalOperator *index_scan_oper = new IndexScanPhysicalOperator(
          table, table_get_oper.table_name(), index, table_get_oper.readonly(), 
          &value, true /*left_inclusive*/, 
          &value, true /*right_inclusive*/);
          
    index_scan_oper->set_predicates(std::move(predicates));
    oper = unique_ptr<PhysicalOperator>(index_scan_oper);
    LOG_TRACE("use index scan");
  } else {
    auto table_scan_oper = new TableScanPhysicalOperator(table, table_get_oper.table_name(), table_get_oper.readonly());
    table_scan_oper->set_predicates(std::move(predicates));
    oper = unique_ptr<PhysicalOperator>(table_scan_oper);
    LOG_TRACE("use table scan");
  }

  return RC::SUCCESS;
}

RC PhysicalPlanGenerator::create_plan(PredicateLogicalOperator &pred_oper, unique_ptr<PhysicalOperator> &oper)
{
  vector<unique_ptr<LogicalOperator>> &children_opers = pred_oper.children();
  ASSERT(children_opers.size() == 1, "predicate logical operator's sub oper number should be 1");

  LogicalOperator &child_oper = *children_opers.front();

  unique_ptr<PhysicalOperator> child_phy_oper;
  RC rc = create(child_oper, child_phy_oper);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to create child operator of predicate operator. rc=%s", strrc(rc));
    return rc;
  }

  vector<unique_ptr<Expression>> &expressions = pred_oper.expressions();
  ASSERT(expressions.size() == 1, "predicate logical operator's children should be 1");

  // 新建了一个PredicatePhysicalOperator，然后求出子节点后做了add_child
  unique_ptr<Expression> expression = std::move(expressions.front());
  oper = unique_ptr<PhysicalOperator>(new PredicatePhysicalOperator(std::move(expression)));
  oper->add_child(std::move(child_phy_oper));
  return rc;
}

RC PhysicalPlanGenerator::create_plan(ProjectLogicalOperator &project_oper, unique_ptr<PhysicalOperator> &oper)
{
  vector<unique_ptr<LogicalOperator>> &child_opers = project_oper.children();

  unique_ptr<PhysicalOperator> child_phy_oper;

  RC rc = RC::SUCCESS;
  if (!child_opers.empty()) {
    LogicalOperator *child_oper = child_opers.front().get();
    rc = create(*child_oper, child_phy_oper);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to create project logical operator's child physical operator. rc=%s", strrc(rc));
      return rc;
    }
  }

  ProjectPhysicalOperator *project_operator = new ProjectPhysicalOperator();


  auto proj_fields = project_oper.get_field_identifiers();
  auto output_names = project_oper.get_output_names();
  for(int i = 0; i < output_names.size(); i++){
    project_operator->add_projection( proj_fields[i],  output_names[i]);
  }

  if (child_phy_oper) {
    project_operator->add_child(std::move(child_phy_oper));
  }

  oper = unique_ptr<PhysicalOperator>(project_operator);

  LOG_TRACE("create a project physical operator");
  return rc;
}

RC PhysicalPlanGenerator::create_plan(InsertLogicalOperator &insert_oper, unique_ptr<PhysicalOperator> &oper)
{
  Table *table = insert_oper.table();
  vector<vector<Value>> value_rows=insert_oper.value_rows();
  //VALUES是在这里传入insert_physical_operator的
  InsertPhysicalOperator *insert_phy_oper = new InsertPhysicalOperator(table, std::move(value_rows));
  //oper指向insert_physical_operator
  oper.reset(insert_phy_oper);
  return RC::SUCCESS;
}

//delete语句的物理计划生成
RC PhysicalPlanGenerator::create_plan(DeleteLogicalOperator &delete_oper, unique_ptr<PhysicalOperator> &oper)
{
  vector<unique_ptr<LogicalOperator>> &child_opers = delete_oper.children();

  unique_ptr<PhysicalOperator> child_physical_oper;

  RC rc = RC::SUCCESS;
  if (!child_opers.empty()) {
    LogicalOperator *child_oper = child_opers.front().get();
    rc = create(*child_oper, child_physical_oper);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to create physical operator. rc=%s", strrc(rc));
      return rc;
    }
  }

  oper = unique_ptr<PhysicalOperator>(new DeletePhysicalOperator(delete_oper.table()));

  if (child_physical_oper) {
    oper->add_child(std::move(child_physical_oper));
  }
  return rc;
}
//update语句的物理计划生成
RC PhysicalPlanGenerator::create_plan(UpdateLogicalOperator &update_oper, unique_ptr<PhysicalOperator> &oper)
{
  vector<unique_ptr<LogicalOperator>> &child_opers = update_oper.children();

  unique_ptr<PhysicalOperator> child_physical_oper;

  RC rc = RC::SUCCESS;
  if (!child_opers.empty()) {
    LogicalOperator *child_oper = child_opers.front().get();
    rc = create(*child_oper, child_physical_oper);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to create physical operator. rc=%s", strrc(rc));
      return rc;
    }
  }

  // 表达式适配算子是一定要有的
  auto adaptor = std::make_unique<AdaptorPhysicalOperatorForExprInSetStmt>();
  AdaptorPhysicalOperatorForExprInSetStmt *p_adaptor = adaptor.get();
  std::unique_ptr<PhysicalOperator> adaptor_top(std::move(adaptor));  // 构建适配算子树时的顶层算子，从adaptor开始

  // 如果set语句中有子查询，则为所有Apply算子生成物理计划
  std::vector<std::unique_ptr<LogicalOperator>> &subquerys = update_oper.subquerys_in_set_stmts();
  if (!subquerys.empty())
  {
    for (auto &subquery: subquerys)
    {
      std::unique_ptr<PhysicalOperator> apply_phy_plan_base;
      rc = this->create(*subquery, apply_phy_plan_base);
      if (rc != RC::SUCCESS)
      {
        LOG_WARN("failed to create apply physical plan for subquerys in set stmt. rc=%s", strrc(rc));
        return rc;
      }

      std::unique_ptr<ApplyPhysicalOperator> apply_phy_plan(
        dynamic_cast<ApplyPhysicalOperator*>(apply_phy_plan_base.release()));
      apply_phy_plan->set_child_op(std::move(adaptor_top));
      adaptor_top = std::move(apply_phy_plan);
    }
  }

  // 保证p_adaptor指向有效的对象，因为它一定在adaptor_top树中
  oper = std::make_unique<UpdatePhysicalOperator>(update_oper.table(), std::move(update_oper.value_list()),
    std::move(adaptor_top), p_adaptor);

  if (child_physical_oper) {
    oper->add_child(std::move(child_physical_oper));
  }
  return rc;
}
RC PhysicalPlanGenerator::create_plan(ExplainLogicalOperator &explain_oper, unique_ptr<PhysicalOperator> &oper)
{
  vector<unique_ptr<LogicalOperator>> &child_opers = explain_oper.children();

  RC rc = RC::SUCCESS;
  unique_ptr<PhysicalOperator> explain_physical_oper(new ExplainPhysicalOperator);
  for (unique_ptr<LogicalOperator> &child_oper : child_opers) {
    unique_ptr<PhysicalOperator> child_physical_oper;
    rc = create(*child_oper, child_physical_oper);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to create child physical operator. rc=%s", strrc(rc));
      return rc;
    }

    explain_physical_oper->add_child(std::move(child_physical_oper));
  }

  oper = std::move(explain_physical_oper);
  return rc;
}

RC PhysicalPlanGenerator::create_plan(JoinLogicalOperator &join_oper, unique_ptr<PhysicalOperator> &oper)
{
  RC rc = RC::SUCCESS;

  vector<unique_ptr<LogicalOperator>> &child_opers = join_oper.children();
  if (child_opers.size() != 2) {
    LOG_WARN("join operator should have 2 children, but have %d", child_opers.size());
    return RC::INTERNAL;
  }

  unique_ptr<PhysicalOperator> join_physical_oper(new NestedLoopJoinPhysicalOperator);
  for (auto &child_oper : child_opers) {
    unique_ptr<PhysicalOperator> child_physical_oper;
    rc = create(*child_oper, child_physical_oper);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to create physical child oper. rc=%s", strrc(rc));
      return rc;
    }

    join_physical_oper->add_child(std::move(child_physical_oper));
  }

  oper = std::move(join_physical_oper);
  return rc;
}

RC PhysicalPlanGenerator::create_plan(CalcLogicalOperator &logical_oper, std::unique_ptr<PhysicalOperator> &oper)
{
  RC rc = RC::SUCCESS;
  CalcPhysicalOperator *calc_oper = new CalcPhysicalOperator(std::move(logical_oper.expressions()));
  oper.reset(calc_oper);
  return rc;
}

RC PhysicalPlanGenerator::create_plan(ScalarSubqueryLogicalOperator &logical_oper, std::unique_ptr<PhysicalOperator> &oper)
{
  std::unique_ptr<LogicalOperator> sub_query_logic_plan = logical_oper.fetch_subquery_logic_plan();

  /* 生成子查询的物理计划 */
  std::unique_ptr<PhysicalOperator> sub_query_phy_plan;
  RC rc = create(*sub_query_logic_plan, sub_query_phy_plan);
  if (rc != RC::SUCCESS)
  {
    LOG_WARN("failed to create subquery physical oper. rc=%s", strrc(rc));
    return rc;
  }
  
  /* 生成子算子的物理计划 */
  auto &child_opers = logical_oper.children();
  std::unique_ptr<PhysicalOperator> child_phy_plan;
  if (child_opers.size() == 1)  // 允许在某些情况下，调用者在生成计划后手动添加子算子
  {
    rc = create(*child_opers.front(), child_phy_plan);
    if (rc != RC::SUCCESS)
    {
      LOG_WARN("failed to create physical child oper. rc=%s", strrc(rc));
      return rc;
    }
  }

  oper = std::make_unique<ApplyPhysicalOperator>(logical_oper.get_field_identifier(), 
    std::move(logical_oper.fetch_correlate_exprs()),
    std::make_unique<ScalarSubqueryEvaluator>(),
    std::move(child_phy_plan), std::move(sub_query_phy_plan));
  
  return rc;
}

RC PhysicalPlanGenerator::create_plan(ExistentialSubqueryLogicalOperator &logical_oper, std::unique_ptr<PhysicalOperator> &oper)
{
  std::unique_ptr<LogicalOperator> sub_query_logic_plan = logical_oper.fetch_subquery_logic_plan();

  /* 生成子查询的物理计划 */
  std::unique_ptr<PhysicalOperator> sub_query_phy_plan;
  RC rc = create(*sub_query_logic_plan, sub_query_phy_plan);
  if (rc != RC::SUCCESS)
  {
    LOG_WARN("failed to create subquery physical oper. rc=%s", strrc(rc));
    return rc;
  }
  
  /* 生成子算子的物理计划 */
  auto &child_opers = logical_oper.children();
  std::unique_ptr<PhysicalOperator> child_phy_plan;
  if (child_opers.size() == 1)  // 允许在某些情况下，调用者在生成计划后手动添加子算子
  {
    rc = create(*child_opers.front(), child_phy_plan);
    if (rc != RC::SUCCESS)
    {
      LOG_WARN("failed to create physical child oper. rc=%s", strrc(rc));
      return rc;
    }
  }

  auto evaluator = std::make_unique<ExistentialSubqueryEvaluator>(logical_oper.get_exist_op());
  oper = std::make_unique<ApplyPhysicalOperator>(logical_oper.get_field_identifier(), 
    std::move(logical_oper.fetch_correlate_exprs()),
    std::move(evaluator),
    std::move(child_phy_plan), std::move(sub_query_phy_plan));
  
  return rc;
}

RC PhysicalPlanGenerator::create_plan(QuantifiedCompSubqueryLogicalOperator &logical_oper, std::unique_ptr<PhysicalOperator> &oper)
{
  std::unique_ptr<LogicalOperator> sub_query_logic_plan = logical_oper.fetch_subquery_logic_plan();

  /* 生成子查询的物理计划 */
  std::unique_ptr<PhysicalOperator> sub_query_phy_plan;
  RC rc = create(*sub_query_logic_plan, sub_query_phy_plan);
  if (rc != RC::SUCCESS)
  {
    LOG_WARN("failed to create subquery physical oper. rc=%s", strrc(rc));
    return rc;
  }
  
  /* 生成子算子的物理计划 */
  auto &child_opers = logical_oper.children();
  std::unique_ptr<PhysicalOperator> child_phy_plan;
  if (child_opers.size() == 1)  // 允许在某些情况下，调用者在生成计划后手动添加子算子
  {
    rc = create(*child_opers.front(), child_phy_plan);
    if (rc != RC::SUCCESS)
    {
      LOG_WARN("failed to create physical child oper. rc=%s", strrc(rc));
      return rc;
    }
  }

  auto evaluator = std::make_unique<QuantifiedCompSubqueryEvaluator>(logical_oper.owns_expr(), 
    logical_oper.get_quantified_comp_op());
  
  oper = std::make_unique<ApplyPhysicalOperator>(logical_oper.get_field_identifier(),
    std::move(logical_oper.fetch_correlate_exprs()),
    std::move(evaluator),
    std::move(child_phy_plan), std::move(sub_query_phy_plan));
  
  return RC::SUCCESS;
}

RC PhysicalPlanGenerator::create_plan(FunctionLogicalOperator& function_oper, std::unique_ptr<PhysicalOperator> &oper){
  vector<unique_ptr<LogicalOperator>> &child_opers = function_oper.children();
  unique_ptr<PhysicalOperator> child_phy_oper;

  RC rc = RC::SUCCESS;
  if (!child_opers.empty()) {
    LogicalOperator *child_oper = child_opers.front().get();
    rc = create(*child_oper, child_phy_oper);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to create function logical operator's child physical operator. rc=%s", strrc(rc));
      return rc;
    }
    oper = make_unique<FunctionPhysicalOperator>(std::move(function_oper.kernel_), function_oper.be_functioned_field_, function_oper.virtual_meta_);
    oper->add_child(std::move(child_phy_oper));
  }else{
    oper = make_unique<FunctionPhysicalOperator>(std::move(function_oper.kernel_), function_oper.be_functioned_field_, function_oper.virtual_meta_);
  }

  return rc;
}

RC PhysicalPlanGenerator::create_plan(FieldCulLogicalOperator& logical_oper, std::unique_ptr<PhysicalOperator> &oper){
  vector<unique_ptr<LogicalOperator>> &child_opers = logical_oper.children();
  unique_ptr<PhysicalOperator> child_phy_oper;

  RC rc = RC::SUCCESS;
  if (!child_opers.empty()) {
    LogicalOperator *child_oper = child_opers.front().get();
    rc = create(*child_oper, child_phy_oper);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to create field cul logical operator's child physical operator. rc=%s", strrc(rc));
      return rc;
    }
    oper = make_unique<FieldCulPhysicalOperator>(logical_oper.field_identifier_, std::move(logical_oper.cul_expr_));
    oper->add_child(std::move(child_phy_oper));
  }else{
    oper =  make_unique<FieldCulPhysicalOperator>(logical_oper.field_identifier_, std::move(logical_oper.cul_expr_));
  }

  return rc;
}

RC PhysicalPlanGenerator::create_plan(InsertMultiLogicalOperator& logical_oper, std::unique_ptr<PhysicalOperator> &oper)
{
  vector<unique_ptr<LogicalOperator>> &child_opers = logical_oper.children();
  unique_ptr<PhysicalOperator> child_phy_oper;

  RC rc = RC::SUCCESS;
  if (!child_opers.empty()) {
    LogicalOperator *child_oper = child_opers.front().get();
    rc = create(*child_oper, child_phy_oper);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to create insert multi logical operator's child physical operator. rc=%s", strrc(rc));
      return rc;
    }
  }

  oper = make_unique<InsertMultiPhysicalOperator>();
  oper->add_child(std::move(child_phy_oper));

  return rc;
}