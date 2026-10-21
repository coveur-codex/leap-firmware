#pragma once
#include "Input.h"
#include <string>
namespace leap {
// Printable ASCII keyboard, including spaces and all password punctuation.
// Credentials entered here stay on this device.
struct WifiEditor {
  enum class Field { Menu, Ssid, Password };
  enum class Action { None, Close, Save, Remove };
  Field field=Field::Menu;
  std::string ssid, password;
  int row=0, character='a', removeIndex=-1;
  std::string glyph() const {
    static const char *extra[]={"ä","ö","ü","Ä","Ö","Ü","ß"};
    return character<=126 ? std::string(1,char(character)) : extra[character-127];
  }
  Action input(const InputEvent &e, int localCount) {
    if (e.longPress) return Action::None;
    if (!e.right) {
      if (e.key == Key::Center) {
        if (field != Field::Menu) field=Field::Menu;
        else return Action::Close;
      }
      if (field != Field::Menu && e.key == Key::Up) {
        if (character>='a' && character<='z') character-=32;
        else if (character>='A' && character<='Z') character+=32;
      }
      if (field != Field::Menu && e.key == Key::Down)
        character=character=='0' ? '!' : '0';
      return Action::None;
    }
    if (field == Field::Menu) {
      if (e.key==Key::Up && row>0) --row;
      if (e.key==Key::Down && row<2+localCount) ++row;
      if (e.key==Key::Center) {
        if (row<2) field=row ? Field::Password : Field::Ssid;
        else if (row==2) return Action::Save;
        else { removeIndex=row-3; return Action::Remove; }
      }
    } else {
      auto &value=field==Field::Ssid ? ssid : password;
      if (e.key==Key::Left) character=character==32 ? 133 : character-1;
      if (e.key==Key::Right) character=character==133 ? 32 : character+1;
      if (e.key==Key::Up && value.size()+glyph().size() <= (field==Field::Ssid ? 32u : 64u)) value+=glyph();
      if (e.key==Key::Down && !value.empty()) {
        size_t last=value.size()-1;
        while (last>0 && (uint8_t(value[last]) & 0xc0)==0x80) --last;
        value.resize(last);
      }
      if (e.key==Key::Center) field=Field::Menu;
    }
    return Action::None;
  }
};
} // namespace leap
