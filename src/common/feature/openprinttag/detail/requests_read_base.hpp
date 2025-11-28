/// @file
#pragma once

#include <string_view>

#include <feature/openprinttag/tool_tag.hpp>
#include <feature/openprinttag/detail/requests_base.hpp>

namespace buddy::openprinttag {

class ReadFieldRequestBase : public Request {

public:
    explicit ReadFieldRequestBase(ToolTagField tag_field)
        : Request(tag_field.section)
        , tag_field_(tag_field) {}

protected:
    const ToolTagField tag_field_;
};

template <typename T, typename Parent = ReadFieldRequestBase>
class ReadRequestMixin : public Parent {

public:
    using Value = T;
    using Error = Request::Error;
    using Result = std::expected<Value, Error>;

public:
    /// Once @p finished, can be used to obtain the result
    /// Cannot be called before finished()
    Result result() const {
        assert(this->finished());

        if (this->has_error()) {
            return std::unexpected(this->error());
        }

        return result_;
    }

protected:
    // Inherit constructors
    using Parent::Parent;

protected:
    Value result_;
};

class ReadInt32FieldRequest : public ReadRequestMixin<int32_t> {

public:
    // Inherit constructor
    using ReadRequestMixin<int32_t>::ReadRequestMixin;
};

template <typename Enum>
class ReadEnumFieldRequest : public ReadRequestMixin<Enum> {

public:
    // Inherit constructor
    using ReadRequestMixin<Enum>::ReadRequestMixin;
};

template <typename Enum>
class ReadEnumArrayFieldRequest : public ReadRequestMixin<std::span<const Enum>> {

public:
    // Inherit constructor
    using ReadRequestMixin<std::span<const Enum>>::ReadRequestMixin;
};

class ReadFloatFieldRequest : public ReadRequestMixin<float> {

public:
    // Inherit constructor
    using ReadRequestMixin<float>::ReadRequestMixin;
};

class ReadStringRequestBase : public ReadRequestMixin<std::string_view> {

public:
    /// @param buffer buffer for storing the text data
    explicit ReadStringRequestBase(ToolTagField tag_field, std::span<char> buffer)
        : ReadRequestMixin(tag_field)
        , buffer_(buffer) {
    }

protected:
    inline void set_buffer(std::span<char> set) {
        buffer_ = set;
    }

private:
    /// Buffer for storing the text data.
    /// The actual result will then be a subspan of this buffer.
    std::span<char> buffer_;
};

template <size_t array_size>
class ReadStringFieldRequest : public ReadStringRequestBase {

public:
    explicit ReadStringFieldRequest(ToolTagField tag_field)
        : ReadStringRequestBase(tag_field, std::span<char> {}) {
        set_buffer(array_);
    }

private:
    std::array<char, array_size> array_;
};

} // namespace buddy::openprinttag
