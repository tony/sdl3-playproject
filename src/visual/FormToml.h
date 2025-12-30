#pragma once

namespace Visual {
struct CharacterForm;
}

namespace Visual {

bool loadFormFromToml(const char* path, CharacterForm& out);

}  // namespace Visual
