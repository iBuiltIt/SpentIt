#pragma once
#include <spentit/import_repository.hpp>
namespace spentit::app {
struct QifExportRange {std::int64_t account;std::string start,end;};
struct QifExportAccount {Account account;std::string first,last;};
std::vector<QifExportAccount> qif_export_accounts(ImportRepository&,const OperationContext&);
std::string export_qif(ImportRepository&,const std::vector<QifExportRange>&,bool categories,bool opening,const OperationContext&);
}
