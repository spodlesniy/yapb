#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace aipb::map::io {

struct Json {
  using Array = std::vector<Json>;
  using Object = std::map<std::string, Json>;
  std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data;

  Json() : data(nullptr) {
  }
  Json(bool value) : data(value) {
  }
  Json(double value) : data(value) {
  }
  Json(std::string value) : data(std::move(value)) {
  }
  Json(const char *value) : data(std::string(value)) {
  }
  Json(Array value) : data(std::move(value)) {
  }
  Json(Object value) : data(std::move(value)) {
  }

  const Object &object() const;
  const Array &array() const;
  const std::string &text() const;
  double number() const;
  bool isNull() const;
  const Json &at(const std::string &key) const;
};

Json parseJson(std::string_view input);
std::string writeJson(const Json &json);

} // namespace aipb::map::io
