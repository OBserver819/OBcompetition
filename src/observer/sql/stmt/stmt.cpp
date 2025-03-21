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
// Created by Wangyunlai on 2022/5/22.
//

#include "common/log/log.h"
#include "sql/stmt/create_table_select_stmt.h"
#include "sql/stmt/stmt.h"
#include "sql/stmt/insert_stmt.h"
#include "sql/stmt/delete_stmt.h"
#include "sql/stmt/update_stmt.h"
#include "sql/stmt/select_stmt.h"
#include "sql/stmt/explain_stmt.h"
#include "sql/stmt/create_index_stmt.h"
#include "sql/stmt/create_table_stmt.h"
#include "sql/stmt/desc_table_stmt.h"
#include "sql/stmt/help_stmt.h"
#include "sql/stmt/show_tables_stmt.h"
#include "sql/stmt/trx_begin_stmt.h"
#include "sql/stmt/trx_end_stmt.h"
#include "sql/stmt/exit_stmt.h"
#include "sql/stmt/set_variable_stmt.h"
#include "sql/stmt/load_data_stmt.h"
#include "sql/stmt/calc_stmt.h"
#include "sql/stmt/drop_table_stmt.h"
#include "sql/stmt/view_stmt.h"
#include "sql/expr/expression.h"
#include "sql/expr/parsed_expr.h"
#include "common/log/log.h"

RC Stmt::create_stmt(Db *db, ParsedSqlNode &sql_node, Stmt *&stmt)
{
  stmt = nullptr;

  switch (sql_node.flag) {
    case SCF_INSERT: {
      return InsertStmt::create(db, sql_node.insertion, stmt);
    }
    case SCF_DELETE: {
      return DeleteStmt::create(db, sql_node.deletion, stmt);
    }
    case SCF_UPDATE: {
      return UpdateStmt::create(db, sql_node.update, stmt);
    }
    case SCF_SELECT: {
      ExprResolveContext ctx;
      std::unordered_map<size_t, std::vector<CorrelateExpr*>> correlate_exprs;
      RC rc = SelectStmt::create(db, &ctx, sql_node.selection, stmt, &correlate_exprs);
      if (rc != RC::SUCCESS) {
        return rc;
      }
      if (!correlate_exprs.empty()) {
        LOG_WARN("correlate expressions are not null after resolve top level select stmt.");
        return RC::INTERNAL;
      }
      return RC::SUCCESS;
    }

    case SCF_EXPLAIN: {
      return ExplainStmt::create(db, sql_node.explain, stmt);
    }

    case SCF_CREATE_INDEX: {
      return CreateIndexStmt::create(db, sql_node.create_index, stmt);
    }

    case SCF_CREATE_TABLE: {
      return CreateTableStmt::create(db, sql_node.create_table, stmt);
    }

    case SCF_DESC_TABLE: {
      return DescTableStmt::create(db, sql_node.desc_table, stmt);
    }

    case SCF_HELP: {
      return HelpStmt::create(stmt);
    }

    case SCF_SHOW_TABLES: {
      return ShowTablesStmt::create(db, stmt);
    }

    case SCF_BEGIN: {
      return TrxBeginStmt::create(stmt);
    }

    case SCF_COMMIT:
    case SCF_ROLLBACK: {
      return TrxEndStmt::create(sql_node.flag, stmt);
    }

    case SCF_EXIT: {
      return ExitStmt::create(stmt);
    }

    case SCF_SET_VARIABLE: {
      return SetVariableStmt::create(sql_node.set_variable, stmt);
    }

    case SCF_LOAD_DATA: {
      return LoadDataStmt::create(db, sql_node.load_data, stmt);
    }

    case SCF_CALC: {
      return CalcStmt::create(sql_node.calc, stmt);
    }

    case SCF_DROP_TABLE: {
      return DropTableStmt::create(sql_node.drop_table, stmt);
    }

    case SCF_CREATE_TABLE_SELECT: {
      return CreateTableSelectStmt::create(db, sql_node.create_table_select, stmt);
    }

    case SCF_CREATE_VIEW: {
      return ViewStmt::create(db, sql_node.create_view, stmt);
    }

    default: {
      LOG_INFO("Command::type %d doesn't need to create statement.", sql_node.flag);
    } break;
  }
  return RC::UNIMPLENMENT;
}
