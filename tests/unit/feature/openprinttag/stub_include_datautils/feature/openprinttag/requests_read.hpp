#pragma once

#include <any>

#include <feature/openprinttag/detail/requests_meta.hpp>

namespace buddy::openprinttag {

using StubData = std::unordered_map<ToolTagField, std::any, ToolTagFieldHash>;
inline thread_local StubData stub_data;

template <CField auto field>
class ReadFieldRequest final : public FieldTypeTraits<FieldTraits<field>::field_type>::template ReadFieldRequest<field> {

public:
    using FieldTraits = openprinttag::FieldTraits<field>;
    using FieldTypeTraits = openprinttag::FieldTypeTraits<FieldTraits::field_type>;
    using BaseReadFieldRequest = FieldTypeTraits::template ReadFieldRequest<field>;

    template <typename... Args>
    explicit inline ReadFieldRequest(ToolTag, Args &&...args)
        : BaseReadFieldRequest(field, std::forward<Args>(args)...) {
        const auto it = stub_data.find(field);
        if (it != stub_data.end()) {
            const auto field_type = typeid(typename BaseReadFieldRequest::Value).name();
            const auto any_type = it->second.type().name();
            CAPTURE(field_type, any_type);
            REQUIRE(field_type == any_type);
            this->result_ = std::any_cast<typename BaseReadFieldRequest::Value>(it->second);
            this->set_finished(std::monostate {});
        } else {
            this->set_finished(std::unexpected(Request::Error::field_not_present));
        }
    }
};

} // namespace buddy::openprinttag
