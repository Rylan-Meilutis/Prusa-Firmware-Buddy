/// @file
#pragma once

// CMSIS macros colliding with MaterialType :/
#undef TPI

#include <openprinttag/opt_defines.hpp>
#include <openprinttag/opt_fields.hpp>

namespace buddy::openprinttag {

using Section = ::openprinttag::Section;
using Region = ::openprinttag::Region;
using Field = ::openprinttag::Field;
using FieldType = ::openprinttag::FieldType;
using MainField = ::openprinttag::MainField;
using AuxField = ::openprinttag::AuxField;

template <typename T>
concept CField = ::openprinttag::CField<T>;

template <CField auto field>
using FieldTraits = ::openprinttag::FieldTraits<field>;

} // namespace buddy::openprinttag
