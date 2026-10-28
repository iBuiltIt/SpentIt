#pragma once
#include <string>
#include <string_view>
namespace spentit {
// Exactly YYYY, MM and DD once each, separated consistently by -, /, . or nothing.
void validate_date_format(std::string_view);
std::string display_date(std::string_view iso_date,std::string_view format);
std::string parse_display_date(std::string_view text,std::string_view format);
std::string suggested_date_format(std::string_view currency);
}
