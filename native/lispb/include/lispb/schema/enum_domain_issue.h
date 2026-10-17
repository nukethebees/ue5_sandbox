#pragma once

#include <lispb/schema/enum_domain_issue_severity.h>

#include <string>

namespace lispb::schema {
struct EnumDomainIssue {
    EnumDomainIssueSeverity severity{EnumDomainIssueSeverity::warning};
    std::string message;
};
}
