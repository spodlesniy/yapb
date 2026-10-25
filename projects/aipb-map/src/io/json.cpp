#include "json.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace aipb::map::io {
namespace {

[[noreturn]] void fail() {
  throw std::runtime_error("invalid JSON document");
}

bool validUtf8(const std::string &value) {
  for (std::size_t i = 0; i < value.size();) {
    const auto first = static_cast<unsigned char>(value[i]);
    if (first < 0x80) {
      ++i;
      continue;
    }
    const unsigned width = first >= 0xf0 && first <= 0xf4 ? 4 : first >= 0xe0 && first <= 0xef ? 3 : first >= 0xc2 && first <= 0xdf ? 2 : 0;
    if (!width || i + width > value.size())
      return false;
    std::uint32_t cp = first & ((1u << (7 - width)) - 1);
    for (unsigned j = 1; j < width; ++j) {
      const auto byte = static_cast<unsigned char>(value[i + j]);
      if ((byte & 0xc0) != 0x80)
        return false;
      cp = (cp << 6) | (byte & 0x3f);
    }
    if (cp < (width == 2 ? 0x80u : width == 3 ? 0x800u : 0x10000u) || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
      return false;
    i += width;
  }
  return true;
}

void appendUtf8(std::string &result, std::uint32_t cp) {
  if (cp <= 0x7f) {
    result += static_cast<char>(cp);
  } else if (cp <= 0x7ff) {
    result += static_cast<char>(0xc0 | (cp >> 6));
    result += static_cast<char>(0x80 | (cp & 0x3f));
  } else if (cp <= 0xffff) {
    result += static_cast<char>(0xe0 | (cp >> 12));
    result += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
    result += static_cast<char>(0x80 | (cp & 0x3f));
  } else {
    result += static_cast<char>(0xf0 | (cp >> 18));
    result += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
    result += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
    result += static_cast<char>(0x80 | (cp & 0x3f));
  }
}

class Parser {
  std::string_view input_;
  std::size_t pos_ {};

  void space() {
    while (pos_ < input_.size() && (input_[pos_] == ' ' || input_[pos_] == '\t' || input_[pos_] == '\r' || input_[pos_] == '\n')) {
      ++pos_;
    }
  }

  char peek() {
    space();
    if (pos_ == input_.size())
      fail();
    return input_[pos_];
  }

  char get() {
    if (pos_ >= input_.size())
      fail();
    return input_[pos_++];
  }

  void expect(char c) {
    space();
    if (get() != c)
      fail();
  }

  std::uint32_t hex4() {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = get();
      value <<= 4;
      if (c >= '0' && c <= '9')
        value |= static_cast<unsigned>(c - '0');
      else if (c >= 'a' && c <= 'f')
        value |= static_cast<unsigned>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F')
        value |= static_cast<unsigned>(c - 'A' + 10);
      else
        fail();
    }
    return value;
  }

  std::string string() {
    expect('"');
    std::string result;
    while (true) {
      const unsigned char c = static_cast<unsigned char>(get());
      if (c == '"')
        break;
      if (c < 0x20)
        fail();
      if (c != '\\') {
        result += static_cast<char>(c);
        continue;
      }
      const char escape = get();
      switch (escape) {
      case '"':
        result += '"';
        break;
      case '\\':
        result += '\\';
        break;
      case '/':
        result += '/';
        break;
      case 'b':
        result += '\b';
        break;
      case 'f':
        result += '\f';
        break;
      case 'n':
        result += '\n';
        break;
      case 'r':
        result += '\r';
        break;
      case 't':
        result += '\t';
        break;
      case 'u': {
        std::uint32_t cp = hex4();
        if (cp >= 0xd800 && cp <= 0xdbff) {
          if (get() != '\\' || get() != 'u')
            fail();
          const auto low = hex4();
          if (low < 0xdc00 || low > 0xdfff)
            fail();
          cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
        } else if (cp >= 0xdc00 && cp <= 0xdfff) {
          fail();
        }
        appendUtf8(result, cp);
        break;
      }
      default:
        fail();
      }
    }
    if (!validUtf8(result))
      fail();
    return result;
  }

  Json number() {
    const auto start = pos_;
    if (input_[pos_] == '-')
      ++pos_;
    if (pos_ == input_.size())
      fail();
    if (input_[pos_] == '0') {
      ++pos_;
    } else {
      if (input_[pos_] < '1' || input_[pos_] > '9')
        fail();
      while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9')
        ++pos_;
    }
    if (pos_ < input_.size() && input_[pos_] == '.') {
      ++pos_;
      if (pos_ == input_.size() || input_[pos_] < '0' || input_[pos_] > '9')
        fail();
      while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9')
        ++pos_;
    }
    if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < input_.size() && (input_[pos_] == '+' || input_[pos_] == '-'))
        ++pos_;
      if (pos_ == input_.size() || input_[pos_] < '0' || input_[pos_] > '9')
        fail();
      while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9')
        ++pos_;
    }
    double value = 0.0;
    const auto digits = input_.substr(start, pos_ - start);
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (parsed.ec != std::errc() || parsed.ptr != digits.data() + digits.size() || !std::isfinite(value))
      fail();
    return Json(value);
  }

  Json value(int depth) {
    if (depth > 64)
      fail();
    const auto c = peek();
    if (c == '"')
      return Json(string());
    if (c == '-' || (c >= '0' && c <= '9'))
      return number();
    if (c == '[') {
      ++pos_;
      Json::Array elements;
      if (peek() == ']') {
        ++pos_;
        return Json(std::move(elements));
      }
      do {
        elements.emplace_back(value(depth + 1));
        if (peek() == ']') {
          ++pos_;
          break;
        }
        expect(',');
      } while (true);
      return Json(std::move(elements));
    }
    if (c == '{') {
      ++pos_;
      Json::Object fields;
      if (peek() == '}') {
        ++pos_;
        return Json(std::move(fields));
      }
      do {
        if (peek() != '"')
          fail();
        const auto key = string();
        expect(':');
        auto element = value(depth + 1);
        if (!fields.emplace(key, std::move(element)).second)
          fail();
        if (peek() == '}') {
          ++pos_;
          break;
        }
        expect(',');
      } while (true);
      return Json(std::move(fields));
    }
    for (const auto &[text, out] : { std::pair { "true", 1 }, { "false", 0 }, { "null", -1 } }) {
      if (input_.substr(pos_, std::char_traits<char>::length(text)) == text) {
        pos_ += std::char_traits<char>::length(text);
        if (out == -1)
          return Json();
        return Json(out == 1);
      }
    }
    fail();
  }

public:
  explicit Parser(std::string_view source) : input_(source) {
  }
  Json parse() {
    auto result = value(0);
    space();
    if (pos_ != input_.size())
      fail();
    return result;
  }
};

void writeString(std::string &output, const std::string &value) {
  if (!validUtf8(value))
    throw std::invalid_argument("invalid UTF-8 in JSON string");
  constexpr char hex[] = "0123456789abcdef";
  output += '"';
  for (const unsigned char c : value) {
    if (c == '"' || c == '\\') {
      output += '\\';
      output += static_cast<char>(c);
    } else if (c < 32) {
      output += "\\u00";
      output += hex[c >> 4];
      output += hex[c & 15];
    } else {
      output += static_cast<char>(c);
    }
  }
  output += '"';
}

void write(std::string &output, const Json &json) {
  if (std::holds_alternative<std::nullptr_t>(json.data))
    output += "null";
  else if (const auto *b = std::get_if<bool>(&json.data))
    output += *b ? "true" : "false";
  else if (const auto *n = std::get_if<double>(&json.data)) {
    if (!std::isfinite(*n))
      throw std::invalid_argument("non-finite JSON number");
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<double>::max_digits10) << *n;
    output += stream.str();
  } else if (const auto *s = std::get_if<std::string>(&json.data))
    writeString(output, *s);
  else if (const auto *array = std::get_if<Json::Array>(&json.data)) {
    output += '[';
    for (std::size_t i = 0; i < array->size(); ++i) {
      if (i)
        output += ',';
      write(output, (*array)[i]);
    }
    output += ']';
  } else {
    output += '{';
    const auto &object = std::get<Json::Object>(json.data);
    bool first = true;
    for (const auto &[key, value] : object) {
      if (!first)
        output += ',';
      first = false;
      writeString(output, key);
      output += ':';
      write(output, value);
    }
    output += '}';
  }
}

} // namespace

const Json::Object &Json::object() const {
  return std::get<Object>(data);
}
const Json::Array &Json::array() const {
  return std::get<Array>(data);
}
const std::string &Json::text() const {
  return std::get<std::string>(data);
}
double Json::number() const {
  return std::get<double>(data);
}
bool Json::isNull() const {
  return std::holds_alternative<std::nullptr_t>(data);
}
const Json &Json::at(const std::string &key) const {
  return object().at(key);
}
Json parseJson(std::string_view input) {
  return Parser(input).parse();
}
std::string writeJson(const Json &json) {
  std::string out;
  write(out, json);
  return out;
}

} // namespace aipb::map::io
