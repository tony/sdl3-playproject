#pragma once

#include "visual/Form.h"

namespace Visual {

// Create a generic procedural "puppet" form with pixel sketch parts
CharacterForm createPuppetForm();

// Create a generic fallback form for undefined characters
CharacterForm createFallbackForm();

}  // namespace Visual
