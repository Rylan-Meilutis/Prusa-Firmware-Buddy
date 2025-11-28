/// @file
#pragma once

#include <feature/openprinttag/detail/requests_meta.hpp>

namespace buddy::openprinttag {

template <CField auto field>
class ReadFieldRequest final : public FieldTypeTraits<FieldTraits<field>::field_type>::template ReadFieldRequest<field> {

public:
    using FieldTraits = openprinttag::FieldTraits<field>;
    using FieldTypeTraits = openprinttag::FieldTypeTraits<FieldTraits::field_type>;
    using BaseReadFieldRequest = FieldTypeTraits::template ReadFieldRequest<field>;

    template <typename... Args>
    explicit inline ReadFieldRequest(ToolTag tag, Args &&...args)
        : BaseReadFieldRequest(tag.field(field), std::forward<Args>(args)...) {
        if constexpr (requires { FieldTraits::example; }) {
            this->result_ = (typename BaseReadFieldRequest::Value)FieldTraits::example;
            this->set_finished(std::monostate {});
        } else {
            this->set_finished(std::unexpected(Request::Error::field_not_present));
        }
    }
};

class EnumerateFieldsRequest final : public ReadRequestMixin<std::span<const Field>, Request> {

public:
    /// @param buffer buffer for storing the text data
    explicit EnumerateFieldsRequest(ToolTag tag, Section section, std::span<Field> buffer)
        : ReadRequestMixin(section_)
        , tag_(tag)
        , section_(section)
        , buffer_(buffer) {
    }

private:
    const ToolTag tag_;

    const Section section_;

    /// Buffer for storing the data.
    /// The actual result will then be a subspan of this buffer.
    std::span<Field> buffer_;
};

} // namespace buddy::openprinttag
