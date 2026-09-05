// SPDX-License-Identifier: Apache-2.0
#ifndef USD_GEOSPATIAL_RESULT_H
#define USD_GEOSPATIAL_RESULT_H

#include <usd_geospatial/diagnostics.h>

#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace usd_geospatial {

/// A value or the typed failure that replaced it.
///
/// Every SDK operation that can fail returns one of these rather than throwing
/// or returning a null handle with an out-of-band error. That is what lets a
/// caller branch on `error().code()` without parsing text, and what gives the
/// language bindings something uniform to translate.
///
/// `Result` is not an exception channel. Calling `value()` on a failed result
/// is a programming error and throws `std::logic_error`; check `ok()` first.
template <typename T>
class Result {
public:
    static Result success(T value) { return Result(std::move(value)); }
    static Result failure(Diagnostic diagnostic) { return Result(std::move(diagnostic)); }

    bool ok() const noexcept { return _state.index() == 0; }
    explicit operator bool() const noexcept { return ok(); }

    const T& value() const {
        _require_ok();
        return std::get<0>(_state);
    }

    T& value() {
        _require_ok();
        return std::get<0>(_state);
    }

    /// Move the value out. The result must not be used for its value again.
    T take() {
        _require_ok();
        return std::move(std::get<0>(_state));
    }

    const Diagnostic& error() const {
        if (ok()) {
            throw std::logic_error("usd_geospatial::Result::error called on a successful result");
        }
        return std::get<1>(_state);
    }

private:
    explicit Result(T value) : _state(std::in_place_index<0>, std::move(value)) {}
    explicit Result(Diagnostic diagnostic) : _state(std::in_place_index<1>, std::move(diagnostic)) {}

    void _require_ok() const {
        if (!ok()) {
            throw std::logic_error(
                std::string("usd_geospatial::Result::value called on a failed result: ") +
                std::get<1>(_state).id());
        }
    }

    std::variant<T, Diagnostic> _state;
};

/// Build a failure of any result type in one expression. The explicit template
/// argument names the value type the caller expected:
/// `fail<RuntimeInfo>(DiagnosticCode::runtime_not_found, Subsystem::sdk, "...")`.
template <typename T>
Result<T> fail(DiagnosticCode code, Subsystem subsystem, std::string message) {
    return Result<T>::failure(Diagnostic(code, subsystem, std::move(message)));
}

}  // namespace usd_geospatial

#endif  // USD_GEOSPATIAL_RESULT_H
