#pragma once
#include <QString>

namespace spentit {

// The explanation shown for a status sentence, or empty when that sentence
// does not get an explanation dialog. The text comes from
// doc/status-message-help-draft.md.
QString statusExplanation(const QString& message);

}
